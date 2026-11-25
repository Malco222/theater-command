#include "Battle/SeaNightCore.h"

#include "Battle/SeaShow.h"

#include <algorithm>
#include <cmath>

namespace SeaNight
{
	namespace
	{
		constexpr double Pi = 3.14159265358979323846;
		constexpr double Deg = Pi / 180.0;

		// ---------------------------------------------------------------------------------------------------------
		// The night's own numbers. [EST] unless marked.

		/** Civil twilight's end: the sun 6 degrees under the horizon ([P] the almanac's definition). */
		constexpr double TwilightDeg = -6.0;
		/** A light comes on: its shutter opens over this many seconds once it is laid, 1 to 4 s after the salvo that
		 * shows her (fleet) or 1 to 3 s after she is in sight (an escort); it comes on searching, swung this far off her
		 * bearing (radians, 3 to 8 degrees, either way), onto her over SwingS seconds. [EST: lights were trained by their
		 * director before the shutter opened, so the search is short.] */
		constexpr double ShutterS = 0.4, DouseS = 0.6, SwingS = 2.5;
		constexpr double SwingLeast = 3.0 * Deg, SwingMost = 8.0 * Deg;
		/** Fleet: held 25 to 60 s, then doused; not lit again for 90 to 180 s (period practice: lights were unmasked
		 * briefly, because a lit ship drew the enemy's fire: Cape Esperance, 11 October 1942, [P] Boise's searchlights
		 * drew the Japanese cruisers' fire; Guadalcanal, 13 November 1942, [P] Hiei's and Akatsuki's lights on Atlanta
		 * drew the American line's, and Akatsuki was sunk; S. E. Morison, History of United States Naval Operations in
		 * World War II, vol. V). A ship whose roll says no tries again 45 s on, while she still fires at close range. */
		constexpr double HoldLeast = 25.0, HoldMore = 35.0, RestLeast = 90.0, RestMore = 90.0, TryAgain = 45.0;
		/** She fires on her still within this many seconds of her last salvo (a battleship's cadence is about 30 s, [SHOW]
		 * Show::Cadence). */
		constexpr double FiringWithin = 35.0;
		/** A beam that loses her (sunk, out of sight or reach, or her guns gone to another): held on where she was last lit
		 * LostHold seconds, then swung LostSwing either side of it at LostRate, out after LostLeast to LostLeast +
		 * LostMore seconds. */
		constexpr double LostHold = 2.0, LostSwing = 10.0 * Deg, LostRate = 4.0 * Deg, LostLeast = 4.0, LostMore = 3.0;
		/** No slot free (MostBeams lit): tried again this many seconds on. */
		constexpr double SlotAgain = 10.0;
		/** A convoy's sweep: an escort within SweepNear metres of a ship of her side struck in the last SweepAfter
		 * seconds sweeps her outer bow (away from her convoy's middle) SweepHalf either side, at SweepRate, for 30 to 50
		 * seconds, one escort in SweepOdds; its beam's axis on the sea at SweepOut of her light's reach. A sweep is laid by
		 * her own place and her side's ships alone. */
		constexpr double SweepNear = 5000.0, SweepAfter = 60.0, SweepOdds = 0.35, SweepHalf = 35.0 * Deg, SweepRate = 5.0 * Deg, SweepOut = 0.8;
		constexpr double SweepLeast = 30.0, SweepMore = 20.0;
		/** Two beams at most on one boat. */
		constexpr int MostOnBoat = 2;
		/** Her aim point: the boat's conning tower, a ship's side up to her upper deck (a share of her length, 3 to 9 m). */
		constexpr double BoatAimUp = 2.5, ShipAimShare = 0.035, ShipAimLeast = 3.0, ShipAimMost = 9.0;
		/** The shaft in the night's air: half seen in clear air (no seeing given: 0.6), whole in a haze that hides a ship at
		 * 2.5 km; a quarter more where it runs through a burning ship's smoke (within SmokeNear metres of its axis). */
		constexpr double ShaftClear = 0.35, ShaftHaze = 2500.0, ShaftUnsaid = 0.6, ShaftSmoke = 0.25, SmokeNear = 600.0;

		/** Fleet star shells. A ship opening fire on a target StarLeast to StarMost metres off fires a spread of them, two
		 * from a destroyer, three from a cruiser or a battleship's secondaries; they burst Beyond (800 to 1,400 m) past
		 * her along the line of fire, StarAcross apart across it, 420 to 560 m up, so she shows black against their light
		 * (US Navy practice: star shells laid beyond the target, to silhouette it). Each burns 48 to 52 s ([P] NavWeaps,
		 * the 5"/38's illuminating rounds "burn for approximately 50 seconds") and sinks under its parachute at [CL]
		 * ConvoyLook::StarFall's 3 m/s. Flown at StarFlight metres a second on average, a tenth of a second apart. A
		 * spread is fired again once the last has burnt out while she still fires on her; a ship whose roll says no tries
		 * again StarAgain seconds on. */
		constexpr double StarLeast = 3500.0, StarMost = 12000.0, BeyondLeast = 800.0, BeyondMore = 600.0, StarAcross = 320.0;
		constexpr double BurstLeast = 420.0, BurstMore = 140.0, BurnLeast = 48.0, BurnMore = 4.0, StarFlight = 650.0, StarAgain = 40.0;

		uint64_t Mix(uint64_t X)
		{
			// (SplitMix64's step: the night's own stream)
			X += 0x9E3779B97F4A7C15ull;
			X = (X ^ (X >> 30)) * 0xBF58476D1CE4E5B9ull;
			X = (X ^ (X >> 27)) * 0x94D049BB133111EBull;
			return X ^ (X >> 31);
		}
		uint64_t KeyOf(int A, int B, int C, int What)
		{
			return Mix(Mix(Mix(static_cast<uint64_t>(static_cast<uint32_t>(A)) ^ 0xA5A5ull) ^ static_cast<uint64_t>(static_cast<uint32_t>(B))) ^ (static_cast<uint64_t>(static_cast<uint32_t>(C)) << 20)) ^
			       static_cast<uint64_t>(What);
		}
		double Clamp(double V, double A, double B) { return V < A ? A : V > B ? B : V; }
		double Flat(const P3& A, const P3& B) { return std::sqrt((A.X - B.X) * (A.X - B.X) + (A.Y - B.Y) * (A.Y - B.Y)); }
		double Dist(const P3& A, const P3& B) { return std::sqrt((A.X - B.X) * (A.X - B.X) + (A.Y - B.Y) * (A.Y - B.Y) + (A.Z - B.Z) * (A.Z - B.Z)); }
		/** The bearing (radians clockwise from north) from A to B. */
		double BearingOf(const P3& A, const P3& B) { return std::atan2(B.X - A.X, B.Y - A.Y); }
		P3 WayOf(double Bearing) { return P3(std::sin(Bearing), std::cos(Bearing), 0.0); }

		/** A ship by her key, from the frame's ships sorted by key. */
		struct Index
		{
			std::vector<std::pair<int, const ShipIn*>> ByKey;
			void Make(const Frame& F)
			{
				ByKey.clear();
				ByKey.reserve(F.Ships.size());
				for (const ShipIn& S : F.Ships) ByKey.emplace_back(S.Key, &S);
				std::sort(ByKey.begin(), ByKey.end(), [](const auto& A, const auto& B) { return A.first < B.first; });
			}
			const ShipIn* Find(int Key) const
			{
				const auto It = std::lower_bound(ByKey.begin(), ByKey.end(), Key, [](const auto& A, int K) { return A.first < K; });
				return It != ByKey.end() && It->first == Key ? It->second : nullptr;
			}
		};

		bool IsBoat(const ShipIn& S) { return S.Class == "ss"; }

		/** Her aim point: where a beam laid on her strikes her. */
		P3 AimOf(const ShipIn& S)
		{
			return P3(S.At.X, S.At.Y, IsBoat(S) ? BoatAimUp : Clamp(ShipAimShare * S.Long, ShipAimLeast, ShipAimMost));
		}
		P3 LampAt(const ShipIn& S, const Lamp& L)
		{
			const P3 Ahead = WayOf(S.Heading * Deg);
			return P3(S.At.X + Ahead.X * L.Along * S.Long, S.At.Y + Ahead.Y * L.Along * S.Long, L.High);
		}

		const Lamp Lamps[] = {
			// The US Navy: a destroyer's 24-inch, a cruiser's and a battleship's 36-inch, high-intensity carbon arcs with a
			// "bluish white" light, the 36-inch burning 150 to 190 amperes ([P] NAVPERS 10548, Electrician's Mate 3,
			// chapter 15). Intensity [EST]: by mirror area from the German 60 cm's 135 and 150 cm's 990 million candelas
			// ([P] the German army's figures, as Wikipedia's "German searchlights of World War II" gives them).
			{ "USA", "dd", "24-inch", 0.61, 1.35e8, 3000.0, 0.0131, 12.0, -0.05 },
			{ "USA", "big", "36-inch", 0.91, 3.0e8, 5000.0, 0.0131, 20.0, 0.0 },
			// The Imperial Japanese Navy: 110 cm on destroyers [EST: some carried 90 cm], 150 cm on cruisers and
			// battleships; a navy that trained hard for night battle with them.
			{ "JAP", "dd", "110 cm", 1.10, 4.5e8, 4500.0, 0.0131, 12.0, -0.05 },
			{ "JAP", "big", "150 cm", 1.50, 9.9e8, 6000.0, 0.0131, 24.0, 0.0 },
			// The Royal Navy: a destroyer's 24-inch [EST: 20-inch on some] (Matapan, 28 March 1941: [P] HMS Greyhound's
			// light on the cruiser Fiume, the battleships firing at under 4,000 yards: Cunningham's despatch), a cruiser's
			// 36-inch, a battleship's 44-inch.
			{ "UK", "dd", "24-inch", 0.61, 1.35e8, 3000.0, 0.0131, 12.0, -0.05 },
			{ "UK", "ca", "36-inch", 0.91, 3.0e8, 5000.0, 0.0131, 20.0, 0.0 },
			{ "UK", "bb", "44-inch", 1.12, 4.6e8, 5500.0, 0.0131, 28.0, 0.05 },
			// The Kriegsmarine: 150 cm on cruisers and battleships ([P] 990 million candelas, the army's 150 cm, above),
			// 60 cm on destroyers [EST].
			{ "GER", "dd", "60 cm", 0.60, 1.35e8, 3000.0, 0.0131, 12.0, -0.05 },
			{ "GER", "big", "150 cm", 1.50, 9.9e8, 6000.0, 0.0131, 26.0, 0.05 },
			// The Regia Marina [EST]: 90 cm on destroyers, 120 cm on cruisers and battleships.
			{ "ITA", "dd", "90 cm", 0.90, 3.0e8, 4000.0, 0.0131, 12.0, -0.05 },
			{ "ITA", "big", "120 cm", 1.20, 5.3e8, 5500.0, 0.0131, 26.0, 0.05 },
			// Any other navy [EST].
			{ "", "dd", "60 cm", 0.60, 1.35e8, 3000.0, 0.0131, 12.0, -0.05 },
			{ "", "big", "90 cm", 0.90, 3.0e8, 4500.0, 0.0131, 20.0, 0.0 },
		};

		std::string NavyOf(const std::string& Nation)
		{
			if (Nation == "USA" || Nation == "US") return "USA";
			if (Nation == "JAP" || Nation == "JPN") return "JAP";
			if (Nation == "UK" || Nation == "GBR") return "UK";
			if (Nation == "GER") return "GER";
			if (Nation == "ITA") return "ITA";
			return "";
		}
	}

	// -------------------------------------------------------------------------------------------------------------
	// The night

	double ShareOf(const Light& L)
	{
		return ConvoyLook::SkyLit(L.bGiven, L.SunUp, L.MoonUp, L.MoonLit);
	}

	bool IsNight(const Light& L, double Share)
	{
		return L.bGiven && L.SunUp < TwilightDeg && Share < 0.5;
	}

	// -------------------------------------------------------------------------------------------------------------
	// Searchlights

	const Lamp& LampOf(const std::string& Class, const std::string& Nation)
	{
		// (a destroyer's own; any other warship a cruiser's, a battleship her own where her navy gave her a bigger one)
		const std::string Navy = NavyOf(Nation);
		const std::string Kind = Class == "dd" ? "dd" : Class == "bb" ? "bb" : "ca";
		const Lamp* Found = nullptr;
		for (const Lamp& L : Lamps)
		{
			const std::string For = L.For;
			if (Navy == L.Navy && (For == Kind || (For == "big" && Kind != "dd")))
			{
				Found = &L;
				break;
			}
		}
		const size_t Any = sizeof(Lamps) / sizeof(Lamps[0]) - (Kind == "dd" ? 2 : 1);
		return Found ? *Found : Lamps[Any];
	}

	bool Lights(const std::string& Class)
	{
		return !Class.empty() && Class != "ss" && Class != "mv" && Class != "cv";
	}

	double UnmaskOdds(const std::string& Nation)
	{
		// [EST] By how each navy fought at night: the Japanese by searchlight and star shell from the first; the British
		// and Italians often (Matapan, Cape Bon); the Germans now and then; the Americans less once their radar laid the
		// guns (from late 1942).
		const std::string Navy = NavyOf(Nation);
		return Navy == "JAP" ? 0.6 : Navy == "UK" ? 0.5 : Navy == "USA" ? 0.3 : 0.4;
	}

	namespace
	{
		double StarOdds(const std::string& Nation)
		{
			// [EST] the share of openings at night beyond searchlight range that fire star shells: the Americans and the
			// Japanese as a drill ([P] early in the war the Japanese star shells burned brighter than the American, as
			// Morison's vol. V tells it), the others less.
			const std::string Navy = NavyOf(Nation);
			return Navy == "USA" || Navy == "JAP" ? 0.8 : Navy == "UK" ? 0.6 : 0.5;
		}
	}

	// -------------------------------------------------------------------------------------------------------------
	// The frame

	void FrameOf(const SeaShow::Show& Show, Frame& Out)
	{
		Out.T = Show.Shown();
		Out.Mine = Show.MineSide();
		const SeaShow::Vec Blow = Show.Wind();
		Out.Wind = P3(Blow.X, Blow.Y, 0.0);
		Out.RulesStars = Show.Checked().StarshellEvents;
		const std::vector<SeaShow::ShipOut>& Ships = Show.Ships();
		Out.Ships.resize(Ships.size());
		for (size_t K = 0; K < Ships.size(); ++K)
		{
			const SeaShow::ShipOut& S = Ships[K];
			ShipIn& In = Out.Ships[K];
			In.Key = S.Key;
			In.Piece = S.Piece;
			In.Side = S.Side;
			In.Class = S.Class;
			In.Nation = S.Nation;
			In.At = P3(S.At.X, S.At.Y, S.At.Z);
			In.Heading = S.Heading;
			In.Long = S.Long;
			In.Beam = S.Beam;
			In.bShown = S.bShown;
			In.bAfloat = S.Doing == SeaShow::Fate::Line || S.Doing == SeaShow::Fate::Limping || (S.Doing == SeaShow::Fate::Sinking && S.Down < 0.33);
			In.bSurfaced = S.Class != "ss" || ((S.Dive.empty() || S.Dive == "surfaced") && S.Dived < 0.5);
			In.bStruck = S.Doing == SeaShow::Fate::Sinking || S.Doing == SeaShow::Fate::Limping || S.bBurning;
			In.Aim = S.Aim;
			In.FiredAt = S.FiredAt;
		}
		const std::map<int, SeaShow::Piece>& Pieces = Show.Pieces();
		Out.Pieces.resize(Pieces.size());
		size_t K = 0;
		for (const auto& Pair : Pieces)
		{
			const SeaShow::Piece& P = Pair.second;
			PieceIn& In = Out.Pieces[K++];
			In.Id = P.Id;
			In.Side = P.Side;
			In.EchoOn = P.EchoOn;
			In.EchoKind = P.EchoKind;
			In.GunOn = P.GunOn;
			In.Heard = P.Heard;
			In.bHeard = P.bHeard;
		}
	}

	bool InSightFor(const Frame& F, int Side, const ShipIn& Her)
	{
		// (what the show does not draw is in no one's sight that the player could see lit)
		if (!Her.bShown) return false;
		// (every ship but a boat is drawn: the show hides only the player's enemy's boats until found)
		if (!IsBoat(Her)) return true;
		// (the player's side: the show's own sight, SetMine, is his)
		if (Side == F.Mine) return true;
		// (the other side, or every boat drawn: only a boat a ship of that side holds, by her view's own words)
		for (const PieceIn& P : F.Pieces)
		{
			if (P.Side == Side && P.EchoOn == Her.Piece && !P.EchoKind.empty() && P.EchoKind != "bearing") return true;
			if (P.Side == Side && P.GunOn == Her.Piece) return true;
		}
		for (const PieceIn& P : F.Pieces)
		{
			if (P.Id != Her.Piece) continue;
			for (const PieceIn& By : F.Pieces)
			{
				if (By.Side != Side) continue;
				if (P.bHeard && std::find(P.Heard.begin(), P.Heard.end(), By.Id) != P.Heard.end()) return true;
				if (P.GunOn == By.Id) return true; // (her gun's flash gives her away)
			}
		}
		return false;
	}

	// -------------------------------------------------------------------------------------------------------------
	// The night of a battle

	double Night::Draw(uint64_t Key) const
	{
		return static_cast<double>(Mix(SeedKey ^ Mix(Key)) >> 11) * (1.0 / 9007199254740992.0);
	}

	void Night::Begin(uint64_t Seed, const Light& InLight, double InShare)
	{
		SeedKey = Mix(Seed ^ 0x6E696768745F7365ull);
		Sky = InLight;
		bShareGiven = InShare >= 0.0;
		DayShare = bShareGiven ? Clamp(InShare, 0.0, 1.0) : ShareOf(Sky);
		bNight = IsNight(Sky, DayShare);
		LastT = -1e9;
		Lighters.clear();
		StruckAt.clear();
		AllStars.clear();
		OutBeams.clear();
		OutStars.clear();
		NextStar = 1;
		Count = Tally();
	}

	void Night::SetShare(double InShare)
	{
		bShareGiven = true;
		DayShare = Clamp(InShare, 0.0, 1.0);
		bNight = IsNight(Sky, DayShare);
	}

	void Night::Reset()
	{
		for (auto& Pair : Lighters)
		{
			Lighter& L = Pair.second;
			L.bLit = false;
			L.bSweep = false;
			L.Target = -1;
			L.NextTry = -1e9;
			L.StarsUntil = -1.0;
		}
		OutBeams.clear();
		LastT = -1e9;
	}

	void Night::Step(const Frame& F)
	{
		// (the clock gone back, or a jump: a picture run's skip; the beams out, the stars kept where they still fly)
		if (LastT > -1e8 && (F.T < LastT - 1e-6 || F.T - LastT > 5.0)) Reset();
		LastT = F.T;
		if (!bNight)
		{
			for (auto& Pair : Lighters) Pair.second.bLit = false;
			OutBeams.clear();
			OutStars.clear();
			return;
		}
		bool bConvoy = false;
		for (const ShipIn& S : F.Ships) bConvoy = bConvoy || S.Class == "ss" || S.Class == "mv";
		StepBeams(F, bConvoy);
		StepStars(F, bConvoy);
	}

	void Night::StepBeams(const Frame& F, bool bConvoy)
	{
		Index Ix;
		Ix.Make(F);
		const double T = F.T;
		for (const ShipIn& S : F.Ships)
		{
			if (S.bStruck && StruckAt.find(S.Key) == StruckAt.end()) StruckAt[S.Key] = T;
		}
		// (each side's merchants' middle: a convoy's sweep looks out from it)
		P3 Middle[2];
		int InConvoy[2] = { 0, 0 };
		for (const ShipIn& S : F.Ships)
		{
			if (S.Class != "mv" || S.Side < 0 || S.Side > 1 || !S.bAfloat) continue;
			Middle[S.Side] = Middle[S.Side] + S.At;
			++InConvoy[S.Side];
		}
		for (int Side = 0; Side < 2; ++Side)
		{
			if (InConvoy[Side] > 0) Middle[Side] = Middle[Side] * (1.0 / InConvoy[Side]);
		}

		// (who holds a boat now: lit beams on her, Searching or Holding)
		auto OnHer = [this](int BoatKey, int Except)
		{
			int N = 0;
			for (const auto& Pair : Lighters)
			{
				const Lighter& O = Pair.second;
				if (Pair.first != Except && O.bLit && O.Target == BoatKey && (O.What == Doing::Searching || O.What == Doing::Holding)) ++N;
			}
			return N;
		};
		auto Holds = [&F](const ShipIn& By, const ShipIn& Boat)
		{
			for (const PieceIn& P : F.Pieces)
			{
				if (P.Id == By.Piece && ((P.EchoOn == Boat.Piece && !P.EchoKind.empty() && P.EchoKind != "bearing") || P.GunOn == Boat.Piece)) return true;
				if (P.Id == Boat.Piece && ((P.bHeard && std::find(P.Heard.begin(), P.Heard.end(), By.Piece) != P.Heard.end()) || P.GunOn == By.Piece)) return true;
			}
			return false;
		};
		// What a lighter would hold now: her target (fleet: the ship she fires on at close range; a convoy's escort: a
		// surfaced boat in sight in her light's reach, one she holds first, then the nearest), or -1.
		auto WantOf = [&](const ShipIn& L, const Lamp& Row) -> const ShipIn*
		{
			const P3 From = LampAt(L, Row);
			if (!bConvoy)
			{
				const ShipIn* Her = L.Aim >= 0 ? Ix.Find(L.Aim) : nullptr;
				if (!Her || Her->Side == L.Side || !Her->bAfloat || !InSightFor(F, L.Side, *Her)) return nullptr;
				if (L.FiredAt < 0.0 || T - L.FiredAt > FiringWithin) return nullptr;
				if (Flat(From, Her->At) > std::min(Row.Reach, CloseRange)) return nullptr;
				return Her;
			}
			const ShipIn* Best = nullptr;
			bool bBestHeld = false;
			double BestFar = 0.0;
			for (const ShipIn& B : F.Ships)
			{
				if (!IsBoat(B) || B.Side == L.Side || !B.bAfloat || !B.bSurfaced || !InSightFor(F, L.Side, B)) continue;
				const double Far = Flat(From, B.At);
				if (Far > Row.Reach || OnHer(B.Key, L.Key) >= MostOnBoat) continue;
				const bool bHeld = Holds(L, B);
				if (!Best || (bHeld && !bBestHeld) || (bHeld == bBestHeld && Far < BestFar))
				{
					Best = &B;
					bBestHeld = bHeld;
					BestFar = Far;
				}
			}
			return Best;
		};
		// Whether the ship a lit beam is on still may be: fleet, she is still the one her guns fire on at close range; an
		// escort's, the boat is still up, afloat, in sight and in reach (the two-a-boat rule counted her when she was taken).
		auto Qualifies = [&](const ShipIn& L, const Lamp& Row, int Key) -> const ShipIn*
		{
			const ShipIn* Her = Key >= 0 ? Ix.Find(Key) : nullptr;
			if (!Her || Her->Side == L.Side || !Her->bAfloat || !InSightFor(F, L.Side, *Her)) return nullptr;
			const P3 From = LampAt(L, Row);
			if (!bConvoy)
			{
				if (L.Aim != Key || L.FiredAt < 0.0 || T - L.FiredAt > FiringWithin) return nullptr;
				return Flat(From, Her->At) <= std::min(Row.Reach, CloseRange) ? Her : nullptr;
			}
			return IsBoat(*Her) && Her->bSurfaced && Flat(From, Her->At) <= Row.Reach ? Her : nullptr;
		};
		auto LitNow = [this]()
		{
			int N = 0;
			for (const auto& Pair : Lighters) N += Pair.second.bLit ? 1 : 0;
			return N;
		};
		// (a hold wanting a place when none is free takes a sweep's)
		auto FreeSlot = [&](bool bForHold)
		{
			if (LitNow() < MostBeams) return true;
			if (!bForHold) return false;
			for (auto& Pair : Lighters)
			{
				Lighter& O = Pair.second;
				if (O.bLit && O.bSweep && O.What != Doing::Dousing)
				{
					// (doused at once: its place is the hold's this frame)
					O.bLit = false;
					O.NextTry = T + SlotAgain;
					++Count.Preempted;
					return true;
				}
			}
			return false;
		};

		for (const ShipIn& L : F.Ships)
		{
			if (!Lights(L.Class)) continue;
			auto Found = Lighters.find(L.Key);
			if (Found == Lighters.end())
			{
				Found = Lighters.emplace(L.Key, Lighter()).first;
				Found->second.LastFired = L.FiredAt;
			}
			Lighter& Me = Found->second;
			const Lamp& Row = LampOf(L.Class, L.Nation);
			if (!L.bAfloat || !L.bShown)
			{
				Me.bLit = false;
				continue;
			}
			const ShipIn* Want = WantOf(L, Row);

			if (!Me.bLit)
			{
				if (T < Me.NextTry) continue;
				if (Want)
				{
					if (!bConvoy)
					{
						++Me.Episode;
						if (Draw(KeyOf(L.Key, Want->Key, Me.Episode, 1)) >= UnmaskOdds(L.Nation))
						{
							Me.NextTry = T + TryAgain;
							continue;
						}
					}
					if (!FreeSlot(true))
					{
						++Count.Refused;
						Me.NextTry = T + SlotAgain;
						continue;
					}
					++Me.Episode;
					Me.bLit = true;
					Me.bSweep = false;
					Me.What = Doing::Searching;
					Me.Target = Want->Key;
					Me.Since = T;
					// (fleet: after the salvo that shows her; an escort: once she is in sight)
					Me.OnAt = (bConvoy ? T : std::max(T, L.FiredAt)) + (bConvoy ? 1.0 + 2.0 * Draw(KeyOf(L.Key, Want->Key, Me.Episode, 2)) : 1.0 + 3.0 * Draw(KeyOf(L.Key, Want->Key, Me.Episode, 2)));
					Me.Until = bConvoy ? 1e18 : Me.OnAt + HoldLeast + HoldMore * Draw(KeyOf(L.Key, Want->Key, Me.Episode, 3));
					const double Swing = SwingLeast + (SwingMost - SwingLeast) * Draw(KeyOf(L.Key, Want->Key, Me.Episode, 4));
					Me.SwingFrom = Draw(KeyOf(L.Key, Want->Key, Me.Episode, 5)) < 0.5 ? -Swing : Swing;
					Me.OutAt = -1.0;
					Me.LostAt = AimOf(*Want);
					Me.LostBearing = BearingOf(LampAt(L, Row), Me.LostAt);
					++Count.Unmasked;
					continue;
				}
				// A convoy's sweep: a ship of her side struck lately, near her, and no boat to hold.
				if (bConvoy)
				{
					int Struck = -1;
					double When = -1.0;
					for (const auto& Pair : StruckAt)
					{
						const ShipIn* S = Ix.Find(Pair.first);
						if (!S || S->Side != L.Side || T - Pair.second > SweepAfter || Pair.second <= Me.StruckSeen) continue;
						if (Flat(S->At, L.At) > SweepNear) continue;
						if (Pair.second > When)
						{
							Struck = Pair.first;
							When = Pair.second;
						}
					}
					if (Struck < 0) continue;
					Me.StruckSeen = When;
					if (Draw(KeyOf(L.Key, Struck, 0, 7)) >= SweepOdds || !FreeSlot(false))
					{
						continue;
					}
					// (outward: from her convoy's middle through her; with no merchants, off her beam away from the ship struck)
					const ShipIn* S = Ix.Find(Struck);
					double Out = 0.0;
					if (L.Side >= 0 && L.Side <= 1 && InConvoy[L.Side] > 0 && Flat(Middle[L.Side], L.At) > 1.0)
					{
						Out = BearingOf(Middle[L.Side], L.At);
					}
					else
					{
						const double Beam = L.Heading * Deg + 0.5 * Pi;
						const P3 Away = WayOf(Beam);
						Out = S && (S->At.X - L.At.X) * Away.X + (S->At.Y - L.At.Y) * Away.Y > 0.0 ? Beam + Pi : Beam;
					}
					++Me.Episode;
					Me.bLit = true;
					Me.bSweep = true;
					Me.What = Doing::Searching;
					Me.Target = -1;
					Me.Since = T;
					Me.OnAt = T + 2.0 + 3.0 * Draw(KeyOf(L.Key, Struck, Me.Episode, 8));
					Me.Until = Me.OnAt + SweepLeast + SweepMore * Draw(KeyOf(L.Key, Struck, Me.Episode, 9));
					Me.SwingFrom = Out;
					Me.SweepHalf = SweepHalf;
					Me.SweepRate = SweepRate;
					Me.OutAt = -1.0;
					++Count.Sweeps;
				}
				continue;
			}

			// Lit: on her, losing her, sweeping or going out.
			if (Me.What == Doing::Dousing)
			{
				if (T >= Me.OutAt + DouseS)
				{
					Me.bLit = false;
					Me.NextTry = bConvoy ? T + 5.0 : T + RestLeast + RestMore * Draw(KeyOf(L.Key, Me.Target, Me.Episode, 6));
				}
				continue;
			}
			if (T >= Me.Until)
			{
				Me.What = Doing::Dousing;
				Me.OutAt = T;
				continue;
			}
			if (Me.bSweep)
			{
				// (a boat to hold takes the sweep's beam onto her)
				if (Want)
				{
					Me.bSweep = false;
					Me.What = Doing::Searching;
					Me.Target = Want->Key;
					Me.Since = T;
					Me.OnAt = std::max(Me.OnAt, T);
					Me.Until = 1e18;
					Me.SwingFrom = 0.0;
					Me.LostAt = AimOf(*Want);
					Me.LostBearing = BearingOf(LampAt(L, Row), Me.LostAt);
					++Count.Unmasked;
				}
				continue;
			}
			const bool bOnHer = Qualifies(L, Row, Me.Target) != nullptr;
			if (Me.What == Doing::Lost)
			{
				if (Want && bConvoy)
				{
					Me.What = Doing::Searching;
					Me.Target = Want->Key;
					Me.Since = T;
					Me.SwingFrom = 0.0;
					Me.Until = 1e18;
					Me.LostAt = AimOf(*Want);
					Me.LostBearing = BearingOf(LampAt(L, Row), Me.LostAt);
				}
				continue;
			}
			if (!bOnHer)
			{
				// (she is lost: the beam stays where she was last lit, then swings about it, then goes out)
				Me.What = Doing::Lost;
				Me.Since = T;
				Me.Until = T + LostLeast + LostMore * Draw(KeyOf(L.Key, Me.Target, Me.Episode, 10));
				continue;
			}
			if (Me.What == Doing::Searching && T >= Me.OnAt + SwingS)
			{
				Me.What = Doing::Holding;
				++Count.Held;
			}
		}

		// The beams as drawn: from each lit lamp, along its axis.
		OutBeams.clear();
		double ShaftAir = ShaftUnsaid;
		if (Sky.Seeing > 0.0)
		{
			ShaftAir = ShaftClear + (1.0 - ShaftClear) * std::min(1.0, ShaftHaze / std::max(Sky.Seeing, 500.0));
		}
		for (const ShipIn& L : F.Ships)
		{
			auto Found = Lighters.find(L.Key);
			if (Found == Lighters.end() || !Found->second.bLit) continue;
			Lighter& Me = Found->second;
			const Lamp& Row = LampOf(L.Class, L.Nation);
			Beam& B = Me.Drawn;
			B.Lighter = L.Key;
			B.Side = L.Side;
			B.What = Me.What;
			B.bSweep = Me.bSweep;
			B.From = LampAt(L, Row);
			B.MirrorM = Row.MirrorM;
			B.Spread = Row.Spread;
			B.Candela = Row.Candela;
			B.Size = Row.Size;
			B.Target = -1;
			B.Laying = Me.bSweep || Me.What == Doing::Lost || Me.What == Doing::Dousing ? -1 : Me.Target;
			const double Up = Clamp((T - Me.OnAt) / ShutterS, 0.0, 1.0);
			const double Out = Me.What == Doing::Dousing ? 1.0 - Clamp((T - Me.OutAt) / DouseS, 0.0, 1.0) : 1.0;
			B.Light = Up * Up * (3.0 - 2.0 * Up) * Out;
			if (Me.bSweep)
			{
				const double Period = 4.0 * Me.SweepHalf / std::max(Me.SweepRate, 1e-6);
				const double Bearing = Me.SwingFrom + Me.SweepHalf * std::sin(2.0 * Pi * (T - Me.OnAt) / Period);
				const P3 Way = WayOf(Bearing);
				const double Far = SweepOut * Row.Reach;
				B.To = P3(B.From.X + Way.X * Far, B.From.Y + Way.Y * Far, 0.0);
			}
			else if (Me.What == Doing::Lost || Me.What == Doing::Dousing)
			{
				// (on where she was last lit; after LostHold, swung about it)
				const double Off = T - Me.Since > LostHold ? LostSwing * std::sin((T - Me.Since - LostHold) * LostRate / LostSwing) : 0.0;
				const double Far = Flat(B.From, Me.LostAt);
				const double Bearing = Me.LostBearing + (Me.What == Doing::Lost ? Off : 0.0);
				const P3 Way = WayOf(Bearing);
				B.To = P3(B.From.X + Way.X * Far, B.From.Y + Way.Y * Far, Me.LostAt.Z);
			}
			else
			{
				const ShipIn* Her = Ix.Find(Me.Target);
				if (Her)
				{
					const P3 Aim = AimOf(*Her);
					const double Swing = Me.What == Doing::Searching ? Me.SwingFrom * (1.0 - ConvoyLook::Smooth(Me.OnAt, Me.OnAt + SwingS, T)) : 0.0;
					const double Bearing = BearingOf(B.From, Aim) + Swing;
					const double Far = Flat(B.From, Aim);
					const P3 Way = WayOf(Bearing);
					B.To = P3(B.From.X + Way.X * Far, B.From.Y + Way.Y * Far, Aim.Z);
					// (only a beam on her names her: one still swinging onto her is not yet on her)
					B.Target = std::fabs(Swing) <= Row.Spread ? Her->Key : -1;
					Me.LostAt = Aim;
					Me.LostBearing = BearingOf(B.From, Aim);
				}
			}
			B.Long = Dist(B.From, B.To);
			// (its shaft: the night's haze, and a burning ship's smoke within SmokeNear of its axis)
			double Smoke = 0.0;
			for (const ShipIn& S : F.Ships)
			{
				if (!S.bStruck || !S.bShown) continue;
				const P3 D(B.To.X - B.From.X, B.To.Y - B.From.Y, 0.0);
				const double Len2 = D.X * D.X + D.Y * D.Y;
				const double U = Len2 > 1e-9 ? Clamp(((S.At.X - B.From.X) * D.X + (S.At.Y - B.From.Y) * D.Y) / Len2, 0.0, 1.0) : 0.0;
				const P3 Near(B.From.X + D.X * U, B.From.Y + D.Y * U, 0.0);
				if (Flat(Near, S.At) < SmokeNear) Smoke = ShaftSmoke;
			}
			B.Shaft = std::min(1.0, ShaftAir + Smoke);
			if (B.Light > 0.0) OutBeams.push_back(B);
		}
		Count.MostBeams = std::max(Count.MostBeams, LitNow());
	}

	void Night::StepStars(const Frame& F, bool bConvoy)
	{
		const double T = F.T;
		// (the dead ones let go: burnt out and their dead candle fallen)
		AllStars.erase(std::remove_if(AllStars.begin(), AllStars.end(), [T](const Star& S) { return T > S.In.Until + 4.0; }), AllStars.end());
		// (the convoy nights' star shells are the rules': never doubled here)
		const bool bFleet = !bConvoy && F.RulesStars == 0;
		Index Ix;
		Ix.Make(F);
		auto Alive = [this, T](int Side)
		{
			int N = 0;
			for (const Star& S : AllStars) N += S.Side == Side && T <= S.In.Until ? 1 : 0;
			return N;
		};
		for (const ShipIn& L : F.Ships)
		{
			if (!Lights(L.Class)) continue;
			auto Found = Lighters.find(L.Key);
			if (Found == Lighters.end()) continue;
			Lighter& Me = Found->second;
			const bool bFired = L.FiredAt > Me.LastFired + 1e-9;
			Me.LastFired = std::max(Me.LastFired, L.FiredAt);
			if (!bFleet || !bFired || !L.bAfloat || !L.bShown) continue;
			const ShipIn* Her = L.Aim >= 0 ? Ix.Find(L.Aim) : nullptr;
			if (!Her || Her->Side == L.Side || !Her->bAfloat || !InSightFor(F, L.Side, *Her)) continue;
			const double Range = Flat(L.At, Her->At);
			if (Range < StarLeast || Range > StarMost) continue;
			if (Me.StarsUntil > T) continue;
			++Me.Spreads;
			if (Draw(KeyOf(L.Key, Her->Key, Me.Spreads, 11)) >= StarOdds(L.Nation))
			{
				Me.StarsUntil = T + StarAgain;
				continue;
			}
			// (a spread is a pair from a destroyer, three from a cruiser or battleship; with no room left for a pair the
			//  side holds its fire rather than send up a single star)
			int N = L.Class == "dd" ? 2 : 3;
			N = std::min(N, MostStarsPerSide - Alive(L.Side));
			if (N < 2)
			{
				++Count.StarsRefused;
				Me.StarsUntil = T + StarAgain;
				continue;
			}
			const P3 Along((Her->At.X - L.At.X) / Range, (Her->At.Y - L.At.Y) / Range, 0.0);
			const P3 Across(Along.Y, -Along.X, 0.0);
			const double Beyond = BeyondLeast + BeyondMore * Draw(KeyOf(L.Key, Her->Key, Me.Spreads, 12));
			double Last = T;
			for (int K = 0; K < N; ++K)
			{
				const int Id = NextStar++;
				const double Side = (K - 0.5 * (N - 1)) * StarAcross + 80.0 * (Draw(KeyOf(L.Key, Id, Me.Spreads, 13)) - 0.5);
				const double Out = Range + Beyond + 150.0 * (Draw(KeyOf(L.Key, Id, Me.Spreads, 14)) - 0.5);
				Star S;
				S.Key = Id;
				S.Side = L.Side;
				S.Firer = L.Key;
				S.Target = Her->Key;
				S.FiredFrom = L.At;
				S.AimAt = Her->At;
				S.In.Burst = P3(L.At.X + Along.X * Out + Across.X * Side, L.At.Y + Along.Y * Out + Across.Y * Side, BurstLeast + BurstMore * Draw(KeyOf(L.Key, Id, Me.Spreads, 15)));
				S.In.T0 = std::max(L.FiredAt, T - 1.0) + Out / StarFlight + 0.5 + 0.1 * K;
				S.In.Until = S.In.T0 + BurnLeast + BurnMore * Draw(KeyOf(L.Key, Id, Me.Spreads, 16));
				S.In.Wind = F.Wind;
				S.In.Seed = Mix(SeedKey ^ static_cast<uint64_t>(Id) * 0x9E3779B97F4A7C15ull) | 1ull;
				AllStars.push_back(S);
				Last = std::max(Last, S.In.Until);
			}
			Me.StarsUntil = Last;
			++Count.Spreads;
			Count.StarsFired += N;
		}
		OutStars = AllStars;
		Count.MostStarsSide = std::max(Count.MostStarsSide, std::max(Alive(0), Alive(1)));
	}
}
