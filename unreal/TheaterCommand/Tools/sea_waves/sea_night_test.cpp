// Night fighting at sea, its looks (Battle/SeaNightCore: the release 2 sea's -TheaterSeaNightFight=, off by default; the
// look bible's S7, "Star shells hang and light the water; gun flashes light the hulls; dark water still shows its waves"),
// held to its brief with no engine:
//  - the night: the night share from the recording's light (ConvoyLook::SkyLit), night only with the sun 6 degrees under;
//  - the searchlights by navy and size;
//  - made-up fleets and convoys: beams only at night, never on a target not in sight (a hidden boat stays hidden: no
//    beam is laid on her, and a sweep is the same with her or without her), at most MostBeams beams and two on a boat,
//    a hold taking a sweep's place, star shells bursting beyond the target along the line of fire at their height, burn
//    and fall, at most MostStarsPerSide burning a side, none by day and none in a convoy (the rules' own);
//  - the battles' own recordings and nights played through the show (SeaShow, as the scene plays them, the player's
//    side as the scene takes it) and the night stand-in (Tools/sea_waves/stand-ins/night-cruisers.json): the same rules
//    frame by frame, nothing at all in a recording with no night, the same beams and stars in two plays;
//  - the cost of a frame for 40 ships;
//  - the effects kit's flash lamps for the naval guns (the scene's "howitzer" row for 180 mm and up, "tank_gun" below):
//    its own core and the recipe the game ships (Content/Effects/battle-fx.json), measured as they are today.
// Usage: sea_night_test <Source/TheaterCommand> <Content> <recording.json ...>

#include "Battle/AirFeed.h"
#include "Battle/ConvoyLooksCore.h"
#include "Battle/SeaFeed.h"
#include "Battle/SeaNightCore.h"
#include "Battle/SeaShow.h"
#include "Effects/TheaterFxCore.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <functional>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace
{
	int Checks = 0, Failed = 0;

	void Check(const std::string& What, bool bOk, const std::string& Got)
	{
		++Checks;
		Failed += bOk ? 0 : 1;
		std::printf("  %s  %s: %s\n", bOk ? "ok  " : "FAIL", What.c_str(), Got.c_str());
	}

	std::string Num(double V, int Places = 2)
	{
		char Text[64];
		std::snprintf(Text, sizeof(Text), "%.*f", Places, V);
		return Text;
	}

	bool ReadFile(const std::string& Path, std::string& Out)
	{
		std::ifstream In(Path, std::ios::binary);
		if (!In) return false;
		std::stringstream Text;
		Text << In.rdbuf();
		Out = Text.str();
		return true;
	}

	std::string BaseOf(const std::string& Path)
	{
		const size_t Slash = Path.find_last_of('/');
		return Slash == std::string::npos ? Path : Path.substr(Slash + 1);
	}

	using SeaNight::P3;
	constexpr double Pi = 3.14159265358979323846;

	double Flat(const P3& A, const P3& B) { return std::hypot(A.X - B.X, A.Y - B.Y); }

	/** FNV-1a over the bits of what was drawn: the same plays give the same number. */
	struct Hash
	{
		uint64_t H = 1469598103934665603ull;
		void Add(const void* Data, size_t N)
		{
			const unsigned char* B = static_cast<const unsigned char*>(Data);
			for (size_t K = 0; K < N; ++K) H = (H ^ B[K]) * 1099511628211ull;
		}
		void Add(double V) { Add(&V, sizeof(V)); }
		void Add(int V) { Add(&V, sizeof(V)); }
	};
	void HashOut(Hash& H, const SeaNight::Night& N, double T)
	{
		H.Add(T);
		for (const SeaNight::Beam& B : N.Beams())
		{
			H.Add(B.Lighter);
			H.Add(B.Target);
			H.Add(B.Laying);
			H.Add(static_cast<int>(B.What));
			H.Add(B.From.X); H.Add(B.From.Y); H.Add(B.To.X); H.Add(B.To.Y); H.Add(B.To.Z);
			H.Add(B.Light);
			H.Add(B.Shaft);
		}
		for (const SeaNight::Star& S : N.Stars())
		{
			H.Add(S.Key);
			H.Add(S.In.Burst.X); H.Add(S.In.Burst.Y); H.Add(S.In.Burst.Z); H.Add(S.In.T0); H.Add(S.In.Until);
		}
	}

	/** The rules every frame is held to, whatever made it: what each beam is on, how many, the stars by side. */
	struct Rules
	{
		long Frames = 0, BeamFrames = 0, OverBeams = 0, OnHidden = 0, OnUnseen = 0, OnOwn = 0, OffReach = 0, ByDay = 0, OverStars = 0;
		long HiddenBoatFrames = 0, HeldOffAim = 0, StarsChecked = 0, StarsShort = 0, StarsWide = 0, StarsLow = 0, StarsBurn = 0;
		long Bad = 0;
		std::set<int> StarsSeen;
		int MostBeams = 0, MostStars = 0;
		void Frame(const SeaNight::Frame& F, const SeaNight::Night& N)
		{
			++Frames;
			const int Lit = static_cast<int>(N.Beams().size());
			MostBeams = std::max(MostBeams, Lit);
			BeamFrames += Lit > 0 ? 1 : 0;
			OverBeams += Lit > SeaNight::MostBeams ? 1 : 0;
			if (!N.IsOn() && (Lit > 0 || !N.Stars().empty())) ++ByDay;
			std::map<int, const SeaNight::ShipIn*> ByKey;
			bool bHiddenBoat = false;
			for (const SeaNight::ShipIn& S : F.Ships)
			{
				ByKey[S.Key] = &S;
				bHiddenBoat = bHiddenBoat || (S.Class == "ss" && !S.bShown);
			}
			HiddenBoatFrames += bHiddenBoat ? 1 : 0;
			for (const SeaNight::Beam& B : N.Beams())
			{
				for (const int On : { B.Target, B.Laying })
				{
					if (On < 0) continue;
					const auto It = ByKey.find(On);
					if (It == ByKey.end()) { ++Bad; continue; }
					const SeaNight::ShipIn& Her = *It->second;
					OnHidden += Her.bShown ? 0 : 1;
					OnUnseen += SeaNight::InSightFor(F, B.Side, Her) ? 0 : 1;
					OnOwn += Her.Side == B.Side ? 1 : 0;
					const auto L = ByKey.find(B.Lighter);
					if (L != ByKey.end())
					{
						const SeaNight::Lamp& Row = SeaNight::LampOf(L->second->Class, L->second->Nation);
						OffReach += Flat(B.From, Her.At) > Row.Reach + 1.0 ? 1 : 0;
					}
				}
				// (held on her: its axis ends at her, within its own spread of her aim)
				if (B.What == SeaNight::Doing::Holding && B.Target >= 0)
				{
					const auto It = ByKey.find(B.Target);
					if (It != ByKey.end() && Flat(B.To, It->second->At) > 1.0) ++HeldOffAim;
				}
			}
			int Burning[2] = { 0, 0 };
			for (const SeaNight::Star& S : N.Stars())
			{
				const ConvoyLook::StarOut O = ConvoyLook::StarAt(S.In, F.T);
				if (O.bBurning && S.Side >= 0 && S.Side <= 1) ++Burning[S.Side];
				if (StarsSeen.insert(S.Key).second) Star(S);
			}
			MostStars = std::max({ MostStars, Burning[0], Burning[1] });
			OverStars += Burning[0] > SeaNight::MostStarsPerSide || Burning[1] > SeaNight::MostStarsPerSide ? 1 : 0;
		}
		/** A star shell as fired: beyond her along the line of fire, at its height, burn and fall. */
		void Star(const SeaNight::Star& S)
		{
			++StarsChecked;
			const double Range = Flat(S.FiredFrom, S.AimAt);
			const double Ux = (S.AimAt.X - S.FiredFrom.X) / Range, Uy = (S.AimAt.Y - S.FiredFrom.Y) / Range;
			const double Along = (S.In.Burst.X - S.FiredFrom.X) * Ux + (S.In.Burst.Y - S.FiredFrom.Y) * Uy;
			const double Across = std::fabs(-(S.In.Burst.X - S.FiredFrom.X) * Uy + (S.In.Burst.Y - S.FiredFrom.Y) * Ux);
			StarsShort += Along < Range + 600.0 || Along > Range + 1600.0 ? 1 : 0;
			StarsWide += Across > 600.0 ? 1 : 0;
			StarsLow += S.In.Burst.Z < 400.0 || S.In.Burst.Z > 600.0 ? 1 : 0;
			const double Burn = S.In.Until - S.In.T0;
			StarsBurn += Burn < 45.0 || Burn > 55.0 ? 1 : 0;
		}
		void Say(const std::string& Name, bool bNightExpected)
		{
			Check(Name + ": at most " + std::to_string(SeaNight::MostBeams) + " beams at once", OverBeams == 0, "most " + std::to_string(MostBeams) + " in " + std::to_string(Frames) + " frames");
			Check(Name + ": no beam on a ship the show does not draw (a hidden boat stays hidden)", OnHidden == 0,
				std::to_string(OnHidden) + " (frames with a hidden boat: " + std::to_string(HiddenBoatFrames) + ")");
			Check(Name + ": no beam on a ship not in its side's sight, nor on its own side, nor past its light's reach", OnUnseen == 0 && OnOwn == 0 && OffReach == 0 && Bad == 0,
				std::to_string(OnUnseen) + " unseen, " + std::to_string(OnOwn) + " own, " + std::to_string(OffReach) + " past reach, " + std::to_string(Bad) + " lost keys");
			Check(Name + ": a held beam ends at her", HeldOffAim == 0, std::to_string(HeldOffAim));
			Check(Name + ": at most " + std::to_string(SeaNight::MostStarsPerSide) + " fleet star shells burning a side", OverStars == 0, "most " + std::to_string(MostStars));
			Check(Name + ": star shells burst 600 to 1,600 m beyond the target along the line of fire, within 600 m of it across, 400 to 600 m up, burning 45 to 55 s",
				StarsShort == 0 && StarsWide == 0 && StarsLow == 0 && StarsBurn == 0,
				std::to_string(StarsChecked) + " checked; " + std::to_string(StarsShort) + " short or far, " + std::to_string(StarsWide) + " wide, " + std::to_string(StarsLow) + " high or low, " + std::to_string(StarsBurn) + " burn");
			if (!bNightExpected) Check(Name + ": nothing by day", ByDay == 0 && BeamFrames == 0 && StarsChecked == 0, std::to_string(BeamFrames) + " frames with beams, " + std::to_string(StarsChecked) + " stars");
		}
	};

	// -------------------------------------------------------------------------------------------------------------
	// Made-up fleets and convoys

	SeaNight::Light NightLight()
	{
		SeaNight::Light L;
		L.bGiven = true;
		L.SunUp = -20.0;
		L.MoonUp = 25.0;
		L.MoonLit = 0.3;
		L.MoonFrom = 140.0;
		L.Seeing = 2500.0;
		return L;
	}
	SeaNight::Light DayLight()
	{
		return SeaNight::Light();
	}

	/** Two lines abreast Gap metres apart steaming east at 10 m/s, each ship laid on her opposite number and firing every
	 * 8 s (her own phase): destroyers, every fifth a cruiser, every seventh a battleship. */
	struct Fleet
	{
		int PerSide = 10;
		double Gap = 3000.0;
		std::string Navy[2] = { "USA", "JAP" };
		std::set<int> Hidden; // (keys drawn hidden, as an enemy boat not found would be)
		void Make(double T, SeaNight::Frame& F) const
		{
			F.T = T;
			F.Mine = 0;
			F.Wind = P3(3.0, 1.0, 0.0);
			F.Ships.clear();
			F.Pieces.clear();
			for (int Side = 0; Side < 2; ++Side)
			{
				for (int I = 0; I < PerSide; ++I)
				{
					SeaNight::ShipIn S;
					S.Key = Side * 1000 + I;
					S.Piece = Side * 100 + I / 3;
					S.Side = Side;
					S.Class = I % 7 == 0 ? "bb" : I % 5 == 0 ? "ca" : "dd";
					S.Nation = Navy[Side];
					S.Long = S.Class == "bb" ? 240.0 : S.Class == "ca" ? 180.0 : 110.0;
					S.Beam = S.Class == "bb" ? 33.0 : S.Class == "ca" ? 20.0 : 11.0;
					S.At = P3(1000.0 + I * 500.0 + 10.0 * T, 5000.0 + Side * Gap, 0.0);
					S.Heading = 90.0;
					S.bShown = Hidden.count(S.Key) == 0;
					S.Aim = (1 - Side) * 1000 + I;
					const double Phase = 0.37 * I + 1.3 * Side;
					S.FiredAt = T < Phase ? -1.0 : Phase + 8.0 * std::floor((T - Phase) / 8.0);
					F.Ships.push_back(S);
				}
			}
		}
	};

	/** A convoy of the player's (side 1, British): four merchants, Escorts destroyers round them, and a German boat (side
	 * 0) surfaced BoatOff metres off the first escort, shown or not as the show would have her, held or not by an echo. */
	struct Convoy
	{
		int Escorts = 2;
		double BoatOff = 1200.0;
		bool bBoat = true, bShown = true, bSurfaced = true, bEcho = false;
		int Mine = 1;
		double StruckAt = -1.0; // (a merchant struck then: the escorts may sweep)
		void Make(double T, SeaNight::Frame& F) const
		{
			F.T = T;
			F.Mine = Mine;
			F.Wind = P3(0.0, 4.0, 0.0);
			F.Ships.clear();
			F.Pieces.clear();
			for (int I = 0; I < 4; ++I)
			{
				SeaNight::ShipIn S;
				S.Key = 100 + I;
				S.Piece = 10 + I;
				S.Side = 1;
				S.Class = "mv";
				S.Nation = "UK";
				S.At = P3(8000.0 + (I % 2) * 600.0, 6000.0 - (I / 2) * 500.0 + 3.6 * T, 0.0);
				S.bStruck = StruckAt >= 0.0 && T >= StruckAt && I == 0;
				F.Ships.push_back(S);
				F.Pieces.push_back(SeaNight::PieceIn{ S.Piece, 1, -1, "", -1, {}, false });
			}
			for (int I = 0; I < Escorts; ++I)
			{
				SeaNight::ShipIn S;
				S.Key = 200 + I;
				S.Piece = 20 + I;
				S.Side = 1;
				S.Class = "dd";
				S.Nation = "UK";
				S.Long = 100.0;
				S.Beam = 10.0;
				const double A = 2.0 * Pi * I / std::max(Escorts, 1);
				S.At = P3(8300.0 + 1500.0 * std::sin(A), 5750.0 + 1500.0 * std::cos(A) + 3.6 * T, 0.0);
				F.Ships.push_back(S);
				SeaNight::PieceIn P{ S.Piece, 1, -1, "", -1, {}, false };
				if (bEcho && I == 0)
				{
					P.EchoOn = 30;
					P.EchoKind = "radar";
				}
				F.Pieces.push_back(P);
			}
			if (bBoat)
			{
				SeaNight::ShipIn B;
				B.Key = 300;
				B.Piece = 30;
				B.Side = 0;
				B.Class = "ss";
				B.Nation = "GER";
				B.Long = 67.0;
				B.Beam = 6.2;
				const SeaNight::ShipIn& First = F.Ships[4];
				B.At = P3(First.At.X + BoatOff, First.At.Y + 200.0, 0.0);
				B.bShown = bShown;
				B.bSurfaced = bSurfaced;
				F.Ships.push_back(B);
				F.Pieces.push_back(SeaNight::PieceIn{ 30, 0, -1, "", -1, {}, false });
			}
		}
	};

	template <typename TMaker>
	void Play(SeaNight::Night& N, const TMaker& Maker, double From, double To, double Fps, Rules* Held, Hash* H, const std::function<void(const SeaNight::Frame&, const SeaNight::Night&)>& Each = nullptr)
	{
		SeaNight::Frame F;
		for (double T = From; T <= To + 1e-9; T += 1.0 / Fps)
		{
			Maker.Make(T, F);
			N.Step(F);
			if (Held) Held->Frame(F, N);
			if (H) HashOut(*H, N, T);
			if (Each) Each(F, N);
		}
	}

	void TheNight()
	{
		std::printf("The night (the recording's light, ConvoyLook::SkyLit)\n");
		const SeaNight::Light Day = DayLight(), Night = NightLight();
		Check("a recording with no light is a day's: share 1, no night", SeaNight::ShareOf(Day) == 1.0 && !SeaNight::IsNight(Day, SeaNight::ShareOf(Day)), Num(SeaNight::ShareOf(Day)));
		const double Share = SeaNight::ShareOf(Night);
		Check("the stand-in's night (sun -20, moon 0.3 lit 25 up): the sky's light on the sea as the convoy looks have it, and night",
			std::fabs(Share - ConvoyLook::SkyLit(true, -20.0, 25.0, 0.3)) < 1e-12 && Share < 0.1 && SeaNight::IsNight(Night, Share), Num(Share, 3));
		SeaNight::Light Dusk = Night;
		Dusk.SunUp = -5.0;
		Check("the sun 5 degrees under: civil twilight, no night fighting's night", !SeaNight::IsNight(Dusk, SeaNight::ShareOf(Dusk)), Num(SeaNight::ShareOf(Dusk), 3));
		SeaNight::Night N;
		N.Begin(7, Night);
		const bool bOn = N.IsOn();
		N.SetShare(0.8);
		const bool bOff = !N.IsOn() && N.Glow() == 0.0;
		N.SetShare(0.1);
		Check("the share handed in (SeaDayCore's UnlitShare, once it lands) turns it off at day's light and on again, the glow 1 less the share", bOn && bOff && N.IsOn() && std::fabs(N.Glow() - 0.9) < 1e-12, Num(N.Glow()));
	}

	void TheLamps()
	{
		std::printf("Searchlights by navy and size\n");
		struct Want { const char* Class; const char* Nation; const char* Size; };
		const Want Rows[] = { { "dd", "USA", "24-inch" }, { "ca", "USA", "36-inch" }, { "bb", "USA", "36-inch" }, { "dd", "JAP", "110 cm" }, { "ca", "JAP", "150 cm" },
			{ "bb", "JAP", "150 cm" }, { "dd", "UK", "24-inch" }, { "ca", "UK", "36-inch" }, { "bb", "UK", "44-inch" }, { "dd", "GER", "60 cm" }, { "bb", "GER", "150 cm" },
			{ "ca", "GER", "150 cm" }, { "dd", "ITA", "90 cm" }, { "bb", "ITA", "120 cm" }, { "dd", "FRA", "60 cm" }, { "ca", "SOV", "90 cm" } };
		std::string Wrong, Said;
		bool bSane = true;
		for (const Want& W : Rows)
		{
			const SeaNight::Lamp L = SeaNight::LampOf(W.Class, W.Nation);
			if (std::string(L.Size) != W.Size) Wrong += std::string(W.Nation) + " " + W.Class + " " + L.Size + "; ";
			bSane = bSane && L.Reach >= 2000.0 && L.Reach <= 7000.0 && L.Candela >= 1e8 && L.Candela <= 1.2e9 && L.MirrorM >= 0.5 && L.MirrorM <= 1.6 && L.Spread > 0.005 && L.Spread < 0.03 && L.High > 5.0;
			Said += std::string(W.Nation) + " " + W.Class + " " + L.Size + " " + Num(L.Reach, 0) + " m; ";
		}
		Check("each navy's lamp by class: the US Navy's 24- and 36-inch, the Japanese 110 and 150 cm, the Royal Navy's 24-, 36- and 44-inch, the Kriegsmarine's 60 and 150 cm, the Italians' 90 and 120 cm",
			Wrong.empty(), Wrong.empty() ? "all" : Wrong);
		Check("every lamp's mirror, intensity, reach (2 to 7 km), pencil and height within reason", bSane, Said);
		Check("lit only by warships: no boat, merchant or carrier", SeaNight::Lights("dd") && SeaNight::Lights("bb") && SeaNight::Lights("ca") && !SeaNight::Lights("ss") && !SeaNight::Lights("mv") && !SeaNight::Lights("cv"), "dd, ca, bb");
	}

	void Fleets()
	{
		std::printf("Fleets by night and by day (made up)\n");
		// Close: 3 km, inside the lights' reach and inside the star shells' least range.
		{
			Fleet Close;
			Close.Gap = 3000.0;
			SeaNight::Night N;
			N.Begin(239, NightLight());
			Rules R;
			Hash H1, H2;
			Play(N, Close, 0.0, 600.0, 30.0, &R, &H1);
			R.Say("two lines at 3 km by night", true);
			const SeaNight::Tally& C = N.Counted();
			Check("two lines at 3 km by night: ships unmask their lights on the ships they fire at, and hold them", C.Unmasked > 0 && C.Held > 0 && R.BeamFrames > 0,
				std::to_string(C.Unmasked) + " unmasked, " + std::to_string(C.Held) + " held, " + std::to_string(R.BeamFrames) + " frames with a beam, " + std::to_string(C.Refused) + " refused for room");
			Check("two lines at 3 km: no star shells inside 3.5 km", C.StarsFired == 0, std::to_string(C.StarsFired));
			SeaNight::Night N2;
			N2.Begin(239, NightLight());
			Play(N2, Close, 0.0, 600.0, 30.0, nullptr, &H2);
			Check("two lines at 3 km: the same beams in two plays", H1.H == H2.H, std::to_string(H1.H));
			SeaNight::Night N3;
			N3.Begin(240, NightLight());
			Hash H3;
			Play(N3, Close, 0.0, 600.0, 30.0, nullptr, &H3);
			Check("another seed, other beams (its own stream, keyed by the seed)", H3.H != H1.H, std::to_string(H3.H));
			// (beams come on only after the salvo that shows her, searching onto her, and go out after 25 to 60 s)
			double FirstOn = 1e9;
			double LongestHold = 0.0;
			std::map<int, double> HeldSince;
			SeaNight::Night N4;
			N4.Begin(239, NightLight());
			Play(N4, Close, 0.0, 600.0, 30.0, nullptr, nullptr, [&](const SeaNight::Frame& F, const SeaNight::Night& Nn)
				{
					std::set<int> Now;
					for (const SeaNight::Beam& B : Nn.Beams())
					{
						FirstOn = std::min(FirstOn, F.T);
						Now.insert(B.Lighter);
						if (!HeldSince.count(B.Lighter)) HeldSince[B.Lighter] = F.T;
						LongestHold = std::max(LongestHold, F.T - HeldSince[B.Lighter]);
					}
					for (auto It = HeldSince.begin(); It != HeldSince.end();) It = Now.count(It->first) ? std::next(It) : HeldSince.erase(It);
				});
			Check("a fleet's light lit no sooner than a second after her first salvo, and out within 70 s (held 25 to 60 s, then lost or doused)", FirstOn >= 1.0 && LongestHold <= 70.0,
				"first at " + Num(FirstOn) + " s, longest " + Num(LongestHold) + " s");
		}
		// Hidden: every ship of side 1 drawn hidden (as an enemy boat not found is): no beam and no star shell on any.
		{
			Fleet Hid;
			Hid.Gap = 3000.0;
			for (int I = 0; I < Hid.PerSide; ++I) Hid.Hidden.insert(1000 + I);
			SeaNight::Night N;
			N.Begin(239, NightLight());
			Rules R;
			Play(N, Hid, 0.0, 300.0, 30.0, &R, nullptr);
			Check("a line the show does not draw: never lit, never a star shell on it", R.OnHidden == 0 && R.OnUnseen == 0 && N.Counted().StarsFired == 0 && R.Bad == 0,
				std::to_string(N.Counted().Unmasked) + " unmasked in all (the other side's, on the drawn line)");
			bool bOnHidden = false;
			for (const SeaNight::Beam& B : N.Beams()) bOnHidden = bOnHidden || B.Laying >= 1000 || B.Target >= 1000;
			Check("a line the show does not draw: no beam laid on it at the end either", !bOnHidden, "none");
		}
		// Far: 7 km, beyond the lights' reach: star shells, beyond her along the line of fire.
		{
			Fleet Far;
			Far.Gap = 7000.0;
			SeaNight::Night N;
			N.Begin(239, NightLight());
			Rules R;
			Hash H1, H2;
			Play(N, Far, 0.0, 600.0, 30.0, &R, &H1);
			R.Say("two lines at 7 km by night", true);
			const SeaNight::Tally& C = N.Counted();
			Check("two lines at 7 km: star shells fired in spreads, and no beam past the lights' reach", C.Spreads > 0 && C.StarsFired >= 2 * C.Spreads && R.BeamFrames == 0,
				std::to_string(C.Spreads) + " spreads, " + std::to_string(C.StarsFired) + " star shells, " + std::to_string(C.StarsRefused) + " refused for the side's room, most burning a side " +
					std::to_string(R.MostStars));
			Check("two lines at 7 km: a side's room for star shells used (the most burning reaches " + std::to_string(SeaNight::MostStarsPerSide) + ")", R.MostStars == SeaNight::MostStarsPerSide,
				std::to_string(R.MostStars));
			// (each star shell flown by the convoy looks' own StarAt: it hangs and sinks about 3 m a second while it burns)
			double WorstFall = 0.0;
			int Flown = 0;
			for (const SeaNight::Star& S : N.Stars())
			{
				const ConvoyLook::StarOut A = ConvoyLook::StarAt(S.In, S.In.T0 + 10.0), B = ConvoyLook::StarAt(S.In, S.In.T0 + 30.0);
				if (!A.bBurning || !B.bBurning) continue;
				++Flown;
				WorstFall = std::max(WorstFall, std::fabs((A.Flare.Z - B.Flare.Z) / 20.0 - 3.0));
			}
			Check("each star shell sinks 3 m a second under its parachute as it burns (ConvoyLook::StarAt)", Flown > 0 && WorstFall < 0.3, std::to_string(Flown) + " flown, worst off 3 m/s by " + Num(WorstFall, 3));
			SeaNight::Night N2;
			N2.Begin(239, NightLight());
			Play(N2, Far, 0.0, 600.0, 30.0, nullptr, &H2);
			Check("two lines at 7 km: the same star shells in two plays", H1.H == H2.H, std::to_string(H1.H));
		}
		// By day: nothing.
		{
			for (const double Gap : { 3000.0, 7000.0 })
			{
				Fleet Day;
				Day.Gap = Gap;
				SeaNight::Night N;
				N.Begin(239, DayLight());
				Rules R;
				Play(N, Day, 0.0, 300.0, 30.0, &R, nullptr);
				R.Say("two lines at " + Num(Gap / 1000.0, 0) + " km by day", false);
			}
			Fleet Dusk;
			SeaNight::Light L = NightLight();
			L.SunUp = -4.0;
			SeaNight::Night N;
			N.Begin(239, L);
			Rules R;
			Play(N, Dusk, 0.0, 300.0, 30.0, &R, nullptr);
			R.Say("two lines at 3 km in civil twilight", false);
		}
		// The budget: 20 a side at 3 km, every ship firing.
		{
			Fleet Big;
			Big.PerSide = 20;
			SeaNight::Night N;
			N.Begin(11, NightLight());
			Rules R;
			Play(N, Big, 0.0, 600.0, 30.0, &R, nullptr);
			Check("40 ships at 3 km, every one firing: at most " + std::to_string(SeaNight::MostBeams) + " beams, the room used and the rest refused", R.OverBeams == 0 && R.MostBeams == SeaNight::MostBeams && N.Counted().Refused > 0,
				"most " + std::to_string(R.MostBeams) + ", " + std::to_string(N.Counted().Refused) + " refused");
		}
	}

	void Convoys()
	{
		std::printf("Convoy nights (made up)\n");
		// A boat up, in the player's sight (shown), 1.2 km off an escort: held.
		{
			Convoy C;
			SeaNight::Night N;
			N.Begin(239, NightLight());
			Rules R;
			int HeldOn = 0;
			Play(N, C, 0.0, 120.0, 30.0, &R, nullptr, [&](const SeaNight::Frame&, const SeaNight::Night& Nn)
				{
					for (const SeaNight::Beam& B : Nn.Beams()) HeldOn += B.Target == 300 && B.What == SeaNight::Doing::Holding ? 1 : 0;
				});
			R.Say("a boat up in sight", true);
			Check("a boat up in sight within an escort's light: held in its beam", HeldOn > 0 && N.Counted().StarsFired == 0, std::to_string(HeldOn) + " beam-frames on her; no fleet star shells in a convoy");
		}
		// The same boat not found (the show hides her): never lit.
		{
			Convoy C;
			C.bShown = false;
			SeaNight::Night N;
			N.Begin(239, NightLight());
			Rules R;
			Play(N, C, 0.0, 120.0, 30.0, &R, nullptr);
			Check("the same boat not found (not drawn): no beam, none laid on her", R.OnHidden == 0 && R.BeamFrames == 0, std::to_string(R.HiddenBoatFrames) + " frames with her hidden");
		}
		// Dived: never lit.
		{
			Convoy C;
			C.bSurfaced = false;
			SeaNight::Night N;
			N.Begin(239, NightLight());
			Rules R;
			Play(N, C, 0.0, 120.0, 30.0, &R, nullptr);
			Check("a boat dived (shown, as for a while after she is found): no beam", R.BeamFrames == 0, std::to_string(R.BeamFrames));
		}
		// The player is the boat (her own side): the escorts light her only once they hold her.
		{
			Convoy C;
			C.Mine = 0;
			SeaNight::Night N;
			N.Begin(239, NightLight());
			Rules R;
			Play(N, C, 0.0, 120.0, 30.0, &R, nullptr);
			Convoy Held = C;
			Held.bEcho = true;
			SeaNight::Night N2;
			N2.Begin(239, NightLight());
			Rules R2;
			Play(N2, Held, 0.0, 120.0, 30.0, &R2, nullptr);
			Check("the player's own boat, up: the escorts light her only once one holds her (its echo on her)", R.BeamFrames == 0 && R2.BeamFrames > 0 && R2.OnUnseen == 0,
				std::to_string(R.BeamFrames) + " frames with no echo, " + std::to_string(R2.BeamFrames) + " with it");
		}
		// Two beams at most on one boat.
		{
			Convoy C;
			C.Escorts = 6;
			C.BoatOff = 300.0;
			SeaNight::Night N;
			N.Begin(239, NightLight());
			int Most = 0;
			Rules R;
			Play(N, C, 0.0, 120.0, 30.0, &R, nullptr, [&](const SeaNight::Frame&, const SeaNight::Night& Nn)
				{
					int On = 0;
					for (const SeaNight::Beam& B : Nn.Beams()) On += B.Laying == 300 ? 1 : 0;
					Most = std::max(Most, On);
				});
			Check("six escorts about a boat up in sight: two beams on her at most", Most == 2, std::to_string(Most));
		}
		// A merchant struck: escorts may sweep; a sweep is the same with a hidden boat or without one; a hold takes a sweep's
		// place when the room is full.
		{
			int Sweeping = 0, Same = 0, Seeds = 0, FullAt = -1;
			for (uint64_t Seed = 1; Seed <= 60; ++Seed)
			{
				Convoy With;
				With.Escorts = 6;
				With.bShown = false;
				With.StruckAt = 10.0;
				Convoy Without = With;
				Without.bBoat = false;
				SeaNight::Night A, B;
				A.Begin(Seed, NightLight());
				B.Begin(Seed, NightLight());
				Hash HA, HB;
				Rules RA;
				int MostSweeps = 0;
				Play(A, With, 0.0, 90.0, 30.0, &RA, &HA, [&](const SeaNight::Frame&, const SeaNight::Night& Nn)
					{
						int S = 0;
						for (const SeaNight::Beam& Bm : Nn.Beams()) S += Bm.bSweep ? 1 : 0;
						MostSweeps = std::max(MostSweeps, S);
					});
				Play(B, Without, 0.0, 90.0, 30.0, nullptr, &HB);
				++Seeds;
				Sweeping += A.Counted().Sweeps > 0 ? 1 : 0;
				Same += HA.H == HB.H && RA.OnHidden == 0 ? 1 : 0;
				if (FullAt < 0 && MostSweeps >= SeaNight::MostBeams) FullAt = static_cast<int>(Seed);
			}
			Check("a merchant struck: the escorts near her sweep with their lights, now and then (35 in a hundred)", Sweeping > 0 && Sweeping < Seeds,
				std::to_string(Sweeping) + " of " + std::to_string(Seeds) + " seeds");
			Check("a sweep is the same with a hidden boat in its arc or without her, for every seed (it reveals nothing)", Same == Seeds, std::to_string(Same) + " of " + std::to_string(Seeds));
			if (FullAt > 0)
			{
				// (the room full of sweeps: a boat surfacing in sight takes a sweep's place)
				struct Surfacing : Convoy
				{
					void MakeAt(double T, SeaNight::Frame& F) const
					{
						Convoy Me = *this;
						Me.bShown = T >= 40.0;
						Me.Make(T, F);
					}
				};
				struct Maker
				{
					Surfacing C;
					void Make(double T, SeaNight::Frame& F) const { C.MakeAt(T, F); }
				} M;
				M.C.Escorts = 6;
				M.C.StruckAt = 10.0;
				SeaNight::Night N;
				N.Begin(static_cast<uint64_t>(FullAt), NightLight());
				Rules R;
				int HeldAfter = 0;
				Play(N, M, 0.0, 90.0, 30.0, &R, nullptr, [&](const SeaNight::Frame& F, const SeaNight::Night& Nn)
					{
						for (const SeaNight::Beam& B : Nn.Beams()) HeldAfter += F.T > 40.0 && B.Laying == 300 ? 1 : 0;
					});
				Check("the room full of sweeps (seed " + std::to_string(FullAt) + "): a boat found takes a sweep's place, never more than " + std::to_string(SeaNight::MostBeams) + " beams",
					N.Counted().Preempted > 0 && HeldAfter > 0 && R.OverBeams == 0, std::to_string(N.Counted().Preempted) + " taken, " + std::to_string(HeldAfter) + " beam-frames on her");
			}
			else
			{
				Check("the room full of sweeps in one of 60 seeds, to try a hold taking a sweep's place", false, "never full");
			}
		}
	}

	// -------------------------------------------------------------------------------------------------------------
	// The battles' own recordings, through the show

	struct Played
	{
		Rules R;
		uint64_t H = 0;
		SeaNight::Tally Count;
		bool bNight = false, bConvoy = false;
		double MeanUs = 0.0, WorstUs = 0.0;
		int MostShips = 0;
	};

	Played PlayRecording(const SeaFeed::Recording& Rec, double Fps)
	{
		Played Out;
		SeaShow::Show Show(Rec.Seed ? Rec.Seed : 239);
		Show.SetField(Rec.Board);
		Show.SetMine(SeaFeed::SideOf(Rec.Views.front().Head.Me));
		const double Mid = Rec.Board.Size * 0.5;
		Show.Eye = SeaShow::Vec(Mid, Mid - 3000.0, 900.0);
		SeaNight::Light L;
		L.bGiven = Rec.Board.bLight;
		L.SunUp = Rec.Board.SunUp;
		L.MoonUp = Rec.Board.MoonUp;
		L.MoonLit = Rec.Board.MoonLit;
		L.MoonFrom = Rec.Board.MoonFrom;
		L.Seeing = Rec.Board.Seeing;
		SeaNight::Night N;
		N.Begin(Rec.Seed ? Rec.Seed : 239, L);
		Out.bNight = N.IsOn();
		size_t Taken = 0;
		auto TakeNext = [&]()
		{
			const SeaFeed::Frame& F = Rec.Views[Taken++];
			Show.Take(F.Head.T, F.Pieces, F.Events, F.bZones ? &F.Zones : nullptr, F.Head.Contact);
		};
		TakeNext();
		Hash H;
		SeaNight::Frame F;
		double Total = 0.0;
		long Frames = 0;
		const double EndT = Rec.Views.back().Head.T;
		for (double T = Rec.Views.front().Head.T; T <= EndT + 1e-9; T += 1.0 / Fps)
		{
			while (Taken < Rec.Views.size() && Rec.Views[Taken - 1].Head.T <= T + 1e-9) TakeNext();
			Show.Advance(T);
			const auto T0 = std::chrono::steady_clock::now();
			SeaNight::FrameOf(Show, F);
			N.Step(F);
			const double Us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - T0).count();
			Total += Us;
			++Frames;
			Out.WorstUs = std::max(Out.WorstUs, Us);
			Out.MostShips = std::max(Out.MostShips, static_cast<int>(F.Ships.size()));
			for (const SeaNight::ShipIn& S : F.Ships) Out.bConvoy = Out.bConvoy || S.Class == "ss" || S.Class == "mv";
			Out.R.Frame(F, N);
			HashOut(H, N, T);
			Show.Effects.clear();
		}
		Out.H = H.H;
		Out.Count = N.Counted();
		Out.MeanUs = Frames ? Total / Frames : 0.0;
		return Out;
	}

	void Recordings(const std::vector<std::string>& Paths)
	{
		for (const std::string& Path : Paths)
		{
			std::string Text, Error;
			SeaFeed::Recording Rec;
			if (!ReadFile(Path, Text) || !SeaFeed::ReadRecording(Text, Rec, Error) || Rec.Views.empty())
			{
				Check(BaseOf(Path) + " read", false, Error.empty() ? "unreadable" : Error);
				continue;
			}
			const std::string Name = BaseOf(Path);
			std::printf("%s (through the show, 30 frames a second)\n", Name.c_str());
			Played A = PlayRecording(Rec, 30.0);
			Played B = PlayRecording(Rec, 30.0);
			A.R.Say(Name, A.bNight);
			Check(Name + ": the same beams and star shells in two plays", A.H == B.H, std::to_string(A.H));
			const SeaNight::Tally& C = A.Count;
			const std::string Said = std::to_string(C.Unmasked) + " unmasked, " + std::to_string(C.Held) + " held, " + std::to_string(C.Sweeps) + " sweeps, " + std::to_string(C.Refused) +
				" refused, " + std::to_string(C.Preempted) + " taken by a hold, " + std::to_string(C.Spreads) + " spreads of " + std::to_string(C.StarsFired) + " star shells, most beams " +
				std::to_string(A.R.MostBeams) + ", most star shells a side " + std::to_string(A.R.MostStars) + "; " + std::to_string(A.R.BeamFrames) + " frames with a beam of " +
				std::to_string(A.R.Frames);
			if (A.bConvoy)
			{
				Check(Name + ": a convoy's night: no fleet star shells (the rules send theirs)", C.StarsFired == 0, Said);
			}
			else if (A.bNight)
			{
				Check(Name + ": a fleet action by night: lights unmasked and held, and star shells fired", C.Unmasked > 0 && C.Held > 0 && C.Spreads > 0, Said);
			}
			else
			{
				Check(Name + ": no night in the recording: nothing lit or fired", C.Unmasked == 0 && C.StarsFired == 0, Said);
			}
			Check(Name + ": a frame's night (the show's frame read and stepped) under 50 microseconds on the mean, " + std::to_string(A.MostShips) + " ships",
				std::min(A.MeanUs, B.MeanUs) < 50.0, Num(std::min(A.MeanUs, B.MeanUs)) + " us mean, worst " + Num(std::min(A.WorstUs, B.WorstUs)) + " us");
		}
	}

	void Cost()
	{
		std::printf("A frame's cost: 40 ships, every one firing, by night\n");
		// (the worse of the two fights, the least of three plays)
		double Worst = 0.0;
		for (const double Gap : { 3000.0, 7000.0 })
		{
			double Least = 1e9;
			for (int Try = 0; Try < 3; ++Try)
			{
				Fleet Big;
				Big.PerSide = 20;
				Big.Gap = Gap;
				SeaNight::Night N;
				N.Begin(5, NightLight());
				SeaNight::Frame F;
				double Total = 0.0;
				long Frames = 0;
				for (double T = 0.0; T <= 300.0; T += 1.0 / 30.0)
				{
					Big.Make(T, F);
					const auto T0 = std::chrono::steady_clock::now();
					N.Step(F);
					Total += std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - T0).count();
					++Frames;
				}
				Least = std::min(Least, Total / Frames);
			}
			Worst = std::max(Worst, Least);
		}
		Check("40 ships, every one firing, at 3 and 7 km: a frame's night under 50 microseconds on the mean (the worse fight, the least of three plays)", Worst < 50.0, Num(Worst) + " us");
	}

	// -------------------------------------------------------------------------------------------------------------
	// The effects kit's flash lamps for the naval guns, as the scene fires them today

	class FlatRecipe final : public TheaterFx::Recipe
	{
	public:
		std::map<std::string, double, std::less<>> Numbers;
		std::map<std::string, std::string, std::less<>> Words;
		void Walk(const AirFeed::Json& J, const std::string& Path)
		{
			switch (J.Kind)
			{
			case AirFeed::Json::Type::Object:
				for (size_t K = 0; K < J.Keys.size(); ++K) Walk(J.Values[K], Path.empty() ? J.Keys[K] : Path + "." + J.Keys[K]);
				break;
			case AirFeed::Json::Type::Array:
				for (size_t K = 0; K < J.Items.size(); ++K) Walk(J.Items[K], Path + "." + std::to_string(K));
				break;
			case AirFeed::Json::Type::Number: Numbers[Path] = J.Value; break;
			case AirFeed::Json::Type::Bool: Numbers[Path] = J.Flag ? 1.0 : 0.0; break;
			case AirFeed::Json::Type::String: Words[Path] = J.Text; break;
			default: break;
			}
		}
		bool Find(const char* Path, double& Out) const override
		{
			const auto It = Numbers.find(std::string_view(Path ? Path : ""));
			if (It == Numbers.end()) return false;
			Out = It->second;
			return true;
		}
		bool FindWord(const char* Path, std::string& Out) const override
		{
			const auto It = Words.find(std::string_view(Path ? Path : ""));
			if (It == Words.end()) return false;
			Out = It->second;
			return true;
		}
	};

	double Sea(void*, double, double) { return 0.0; }

	/** One broadside's lamps as the kit lights them: the scene's Muzzle calls for her turrets (SeaBattle.cpp's Salvo case:
	 * her guns' row by calibre, at her beam's 0.35 up, 0.45 of her beam toward the target), the eye EyeFar metres off,
	 * drawn once, its lamps asked with the day's share; an oil fire burning OilFar off, if any. */
	struct LampsSeen
	{
		int N = 0;
		double Candela[4] = { 0, 0, 0, 0 }, Reach[4] = { 0, 0, 0, 0 };
		bool bOil[4] = { false, false, false, false };
	};
	LampsSeen Broadside(const FlatRecipe& Book, const char* Gun, int Turrets, double EyeFar, double Day, double OilReach)
	{
		TheaterFx::Battle Kit;
		Kit.Setup(&Book, &Sea, nullptr, 3u);
		Kit.SetDrawBeyond(20000.0);
		const double Now = 100.0;
		if (OilReach > 0.0) Kit.OilFire(Now - 30.0, 77, TheaterFx::Vec(EyeFar + 6000.0, 3000.0, 0.0), OilReach, "sunk", Now + 300.0, 20.0);
		// (a battleship 240 m long, beam 33: her turrets at 0.30, 0.17 and -0.28 of her length; a destroyer's at 0.30, -0.22)
		const double Along[3] = { 0.30, 0.17, -0.28 };
		const double Long = Turrets == 3 ? 240.0 : 110.0, Beam = Turrets == 3 ? 33.0 : 11.0;
		for (int K = 0; K < Turrets; ++K)
		{
			const TheaterFx::Vec At(EyeFar + Along[K] * Long, Beam * 0.45, std::max(Beam * 0.35, 4.0));
			Kit.Muzzle(Now, At, TheaterFx::Vec(0.0, 0.95, 0.3), Gun, "water");
		}
		TheaterFx::Battle::View Eye;
		Eye.Eye = TheaterFx::Vec(0.0, -300.0, 25.0);
		Eye.Forward = TheaterFx::Vec(1.0, 0.0, 0.0);
		Eye.Right = TheaterFx::Vec(0.0, -1.0, 0.0);
		Eye.Up = TheaterFx::Vec(0.0, 0.0, 1.0);
		Eye.PixelsPerRadian = 1715.0;
		Eye.HalfAcross = 0.466;
		Kit.Draw(Now + 0.022, Eye);
		TheaterFx::Battle::Lamp Lit[4];
		double Asked = 2.0;
		Book.Find("game.max_lamps", Asked);
		LampsSeen Out;
		Out.N = Kit.Lamps(Now + 0.022, Lit, static_cast<int>(Asked), Day);
		double Candela = 0.0;
		Book.Find("game.lamp_candela_at_full", Candela);
		for (int K = 0; K < Out.N; ++K)
		{
			Out.Candela[K] = Lit[K].Strength * Candela;
			Out.Reach[K] = Lit[K].Radius;
			Out.bOil[K] = Lit[K].At.X > EyeFar + 1000.0;
		}
		return Out;
	}

	void GunLamps(const std::string& Content)
	{
		std::printf("The effects kit's flash lamps for the naval guns (Content/Effects/battle-fx.json, as the scene fires them today)\n");
		std::string Text, Error;
		AirFeed::Json Root;
		if (!ReadFile(Content + "/Effects/battle-fx.json", Text) || !AirFeed::Parse(Text, Root, Error))
		{
			Check("the recipe read", false, Error.empty() ? "unreadable" : Error);
			return;
		}
		FlatRecipe Book;
		Book.Walk(Root, "");
		double Lamps = 0.0, Candela = 0.0, Share = 0.0;
		Book.Find("game.max_lamps", Lamps);
		Book.Find("game.lamp_candela_at_full", Candela);
		Book.Find("game.day_lamp_share", Share);
		Check("the kit's lamps: 2 at a time, 400 cd at full (a 105 mm shell's flash), a quarter by day", Lamps == 2.0 && Candela == 400.0 && Share == 0.25,
			Num(Lamps, 0) + " lamps, " + Num(Candela, 0) + " cd, day share " + Num(Share));
		// (the scene's Effects->Tick passes White today, which the kit takes as full day: DayOf(White))
		TheaterFx::Battle Probe;
		Probe.Setup(&Book, &Sea, nullptr, 3u);
		const double Today = Probe.DayOf(TheaterFx::Rgb(1.0f, 1.0f, 1.0f));
		Check("today the scene hands the kit White, which it reads as full day (step 11 hands it the hour's light)", Today == 1.0, Num(Today));
		for (const double Far : { 2000.0, 800.0 })
		{
			const LampsSeen Heavy = Broadside(Book, "howitzer", 3, Far, 0.0, 0.0), Light = Broadside(Book, "tank_gun", 2, Far, 0.0, 0.0);
			Check("night, a battleship's broadside (\"howitzer\", 180 mm and up) " + Num(Far, 0) + " m from the eye: 2 of its 3 turrets lit, 92.6 cd each, 31.7 m reach",
				Heavy.N == 2 && std::fabs(Heavy.Candela[0] - 92.6) < 0.5 && std::fabs(Heavy.Reach[0] - 31.7) < 0.2,
				std::to_string(Heavy.N) + " lamps, " + Num(Heavy.Candela[0], 1) + " cd, reach " + Num(Heavy.Reach[0], 1) + " m");
			Check("night, a destroyer's broadside (\"tank_gun\", under 180 mm) " + Num(Far, 0) + " m from the eye: both turrets lit, 59.6 cd each, 21.8 m reach",
				Light.N == 2 && std::fabs(Light.Candela[0] - 59.6) < 0.5 && std::fabs(Light.Reach[0] - 21.8) < 0.2,
				std::to_string(Light.N) + " lamps, " + Num(Light.Candela[0], 1) + " cd, reach " + Num(Light.Reach[0], 1) + " m");
		}
		const LampsSeen HeavyToday = Broadside(Book, "howitzer", 3, 2000.0, Today, 0.0), LightToday = Broadside(Book, "tank_gun", 2, 2000.0, Today, 0.0);
		Check("today (White, full day) at night: the heavy broadside's lamps a quarter (23.2 cd, warm white); the destroyer's none (night-only rows)",
			HeavyToday.N == 2 && std::fabs(HeavyToday.Candela[0] - 23.2) < 0.3 && LightToday.N == 0,
			Num(HeavyToday.Candela[0], 1) + " cd; destroyer " + std::to_string(LightToday.N) + " lamps");
		// (a burning ship's oil 6 km beyond: its lamp is 0.25 times the root of its reach, and the two lamps go to the strongest)
		const LampsSeen Crowded = Broadside(Book, "howitzer", 3, 2000.0, 0.0, 60.0);
		int OilLamps = 0;
		for (int K = 0; K < Crowded.N; ++K) OilLamps += Crowded.bOil[K] ? 1 : 0;
		Check("night, the same broadside with a ship's oil burning 6 km further off (60 m): the far fire takes a lamp first (the kit ranks by strength, never by the eye)",
			Crowded.N == 2 && OilLamps == 1 && Crowded.bOil[0], std::to_string(OilLamps) + " of " + std::to_string(Crowded.N) + " to the oil, " + Num(Crowded.Candela[0], 0) + " cd");
	}
}

int main(int Argc, char** Argv)
{
	if (Argc < 3)
	{
		std::fprintf(stderr, "usage: sea_night_test <Source/TheaterCommand> <Content> <recording.json ...>\n");
		return 2;
	}
	const std::string Content = Argv[2];
	std::vector<std::string> Paths;
	for (int K = 3; K < Argc; ++K) Paths.push_back(Argv[K]);
	TheNight();
	TheLamps();
	Fleets();
	Convoys();
	Cost();
	GunLamps(Content);
	Recordings(Paths);
	std::printf("sea_night_test: %d checks, %d failed\n", Checks, Failed);
	return Failed == 0 ? 0 : 1;
}
