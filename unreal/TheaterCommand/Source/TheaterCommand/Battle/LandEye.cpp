#include "Battle/LandEye.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <map>

namespace LandEye
{
	using LandFeed::Track;

	namespace
	{
		constexpr double Pi = 3.14159265358979323846;
		/** How far over the ears of corn a lens near the ground is held, metres. */
		constexpr double OverEars = 0.12;
		/** Corn or grass taller than this round a man down is no place for a low shot: a shoulder shot instead. */
		constexpr double LowInCrop = 0.45;
		/** How far in front of a lens near the ground its stalks are looked for, metres (short of what it looks at). */
		constexpr double AheadSteps[] = { 0.6, 1.2, 1.8, 2.5 };
		/** How many shots a cut may try before it settles for the closing kind (each try asks the lens round). */
		constexpr int TriesACut = 16;
		/** A wide shot's drift across, metres a real second (a crane's slow move). */
		constexpr double WideDrift = 2.5;
		/** The whole mass (Settings::MassFrom): the subject its option is placed on (no track), and how many real seconds
		 * before its window closes the shot on the screen is left for it. */
		constexpr int MassSubject = -2;
		constexpr double MassClosing = 6.0;

		bool IsDown(const std::string& Pose)
		{
			return Pose == "fire_prone" || Pose == "hide" || Pose == "observe" || Pose == "crawl" || Pose == "cower" || Pose == "lmg_burst";
		}

		double WeightOf(const std::string& W)
		{
			if (W == "rifle") return 0.6;
			if (W == "mg") return 1.0;
			if (W == "mortar") return 3.0;
			if (W == "gun") return 4.0;
			if (W == "tank" || W == "at") return 3.0;
			return 1.0;
		}

		Vec Before(double Facing, double Far)
		{
			return Vec(std::sin(Facing) * Far, std::cos(Facing) * Far, 0.0);
		}

		/** A point Back metres behind, Side to the right of, a place facing Facing. */
		Vec Offset(double Facing, double BackM, double SideM)
		{
			const double Fx = std::sin(Facing), Fy = std::cos(Facing);
			// (right of a facing, 0 north clockwise: east when facing north)
			const double Rx = Fy, Ry = -Fx;
			return Vec(-Fx * BackM + Rx * SideM, -Fy * BackM + Ry * SideM, 0.0);
		}

		Vec Ease(const Vec& A, const Vec& B, double Seconds, double Dt)
		{
			if (Seconds <= 1e-6) return B;
			const double U = 1.0 - std::exp(-Dt / Seconds);
			return A + (B - A) * U;
		}

		double Flat(const Vec& A, const Vec& B)
		{
			return std::hypot(A.X - B.X, A.Y - B.Y);
		}

		/** The ways the lens is looked round: six along the axes and eight between. */
		const Vec& RoundWay(int K)
		{
			static const double D = 1.0 / std::sqrt(3.0);
			static const Vec Ways[14] = {
				Vec(1, 0, 0), Vec(-1, 0, 0), Vec(0, 1, 0), Vec(0, -1, 0), Vec(0, 0, 1), Vec(0, 0, -1),
				Vec(D, D, D), Vec(D, D, -D), Vec(D, -D, D), Vec(D, -D, -D), Vec(-D, D, D), Vec(-D, D, -D), Vec(-D, -D, D), Vec(-D, -D, -D),
			};
			return Ways[K];
		}
	}

	const char* NameOf(Kind K)
	{
		switch (K)
		{
		case Kind::Open: return "open";
		case Kind::Wide: return "wide";
		case Kind::Watch: return "watch";
		case Kind::Shoulder: return "shoulder";
		case Kind::Low: return "low";
		case Kind::Tank: return "tank";
		case Kind::Track: return "track";
		}
		return "?";
	}

	const char* NameOf(Why W)
	{
		switch (W)
		{
		case Why::Start: return "start";
		case Why::Opening: return "opening";
		case Why::Length: return "length";
		case Why::Fall: return "fall";
		case Why::Action: return "action";
		case Why::Dead: return "dead";
		case Why::Rescue: return "rescue";
		case Why::Asked: return "asked";
		case Why::Gone: return "gone";
		case Why::Burst: return "burst";
		case Why::Lost: return "lost";
		case Why::Dark: return "dark";
		}
		return "?";
	}

	bool InPicture(const Pose& P, const Vec& At, double Margin, double Aspect)
	{
		const Vec Way = P.Look - P.Eye;
		if (Way.Length() < 1e-6) return false;
		const Vec Fwd = Way.Unit();
		const Vec To = At - P.Eye;
		const double Depth = To.Dot(Fwd);
		if (Depth < 0.5) return false;
		Vec Right = Vec(Fwd.Y, -Fwd.X, 0.0);
		Right = Right.Length() < 1e-6 ? Vec(1.0, 0.0, 0.0) : Right.Unit();
		const Vec Up(Right.Y * Fwd.Z - Right.Z * Fwd.Y, Right.Z * Fwd.X - Right.X * Fwd.Z, Right.X * Fwd.Y - Right.Y * Fwd.X);
		const double TanV = std::tan(P.Fov * 0.5 * Pi / 180.0) * Margin;
		const double TanH = TanV * Aspect;
		return std::abs(To.Dot(Right)) / Depth <= TanH && std::abs(To.Dot(Up)) / Depth <= TanV;
	}

	double Director::Roll()
	{
		Dice += 0x9E3779B97F4A7C15ull;
		uint64_t Z = Dice;
		Z = (Z ^ (Z >> 30)) * 0xBF58476D1CE4E5B9ull;
		Z = (Z ^ (Z >> 27)) * 0x94D049BB133111EBull;
		Z ^= Z >> 31;
		return static_cast<double>(Z >> 11) * (1.0 / 9007199254740992.0);
	}

	double Director::HeightAt(double X, double Y) const
	{
		++RaySteps;
		if (Here.Height) return Here.Height(X, Y);
		return Rec ? Rec->Ground.HeightAt(X, Y) : 0.0;
	}

	bool Director::WhereIs(int TrackIndex, double T, Vec& OutAt, double& OutFacing) const
	{
		if (TrackIndex < 0 || !Rec) return false;
		if (Here.Where && Here.Where(TrackIndex, T, OutAt, OutFacing))
		{
			// (the scene's cores say where he is now; asked of a moment ahead, as the looks ahead do, he is where the
			// recording moves him from there: the scene's Where reads only now, so a look 1.5 s ahead saw him stood still)
			if (LastBattle >= 0.0 && std::abs(T - LastBattle) > 0.05)
			{
				const Track& Tm = Rec->Tracks[static_cast<size_t>(TrackIndex)];
				const LandFeed::Spot A = LandFeed::SpotAt(Tm, LastBattle), B = LandFeed::SpotAt(Tm, T);
				const double Was = HeightAt(OutAt.X, OutAt.Y);
				OutAt.X += B.X - A.X;
				OutAt.Y += B.Y - A.Y;
				OutAt.Z += HeightAt(OutAt.X, OutAt.Y) - Was;
			}
			return true;
		}
		const Track& Tr = Rec->Tracks[static_cast<size_t>(TrackIndex)];
		const LandFeed::Spot S = LandFeed::SpotAt(Tr, T);
		OutAt = Vec(S.X, S.Y, HeightAt(S.X, S.Y));
		OutFacing = S.F;
		// A man firing looks where he fires.
		if (const LandFeed::Change* C = LandFeed::ChangeAt(Tr, T))
			if (C->bAim) OutFacing = LandFeed::FacingTo(S.X, S.Y, C->AimX, C->AimY);
		return true;
	}

	double Director::FocusWanted(double Battle) const
	{
		if (Set.FocusFar <= 0.0) return 0.0;
		Vec At;
		// (past a man's shoulder what he looks at is sharp and he is soft, as the shot is meant; on a watch, a low shot or
		// a tank tracked, the man or the tank itself; a wide, the opening and a ride on a tank all sharp)
		if (Shot == Kind::Shoulder) At = Now.Look;
		else if ((Shot == Kind::Watch || Shot == Kind::Track || Shot == Kind::Low) && Subject >= 0)
		{
			if (!BodyOf(Subject, Battle, At)) return 0.0;
		}
		else return 0.0;
		return (At - Now.Eye).Length();
	}

	bool Director::BodyOf(int TrackIndex, double T, Vec& OutAt) const
	{
		if (TrackIndex < 0 || !Rec) return false;
		if (Here.Body && Here.Body(TrackIndex, T, OutAt)) return true;
		double Facing = 0.0;
		if (!WhereIs(TrackIndex, T, OutAt, Facing)) return false;
		const Track& Tr = Rec->Tracks[static_cast<size_t>(TrackIndex)];
		const bool bFallen = Tr.Fell >= 0.0 && Tr.Fell <= T;
		if (Tr.IsVehicle() || bFallen)
		{
			OutAt.Z += Tr.IsVehicle() ? 1.3 : 0.2;
			return true;
		}
		// (by his stance: lying, his chest some 0.9 m on from his feet, low; kneeling; standing)
		const LandFeed::Change* C = LandFeed::ChangeAt(Tr, T);
		const std::string Pose = C ? C->Pose : std::string("idle");
		if (IsDown(Pose)) OutAt = OutAt + Before(Facing, 0.9) + Vec(0.0, 0.0, 0.3);
		else OutAt.Z += Pose == "fire_kneel" || Pose == "wait" ? 0.8 : 1.25;
		return true;
	}

	int Director::TrackOfShooter(int Block, int K, int J) const
	{
		const int64_t Key = (static_cast<int64_t>(Block) * 65536 + K) * 256 + J;
		const auto It = std::lower_bound(ShooterOf.begin(), ShooterOf.end(), std::make_pair(Key, -1));
		return It != ShooterOf.end() && It->first == Key ? It->second : -1;
	}

	bool Director::TankMoving(double Battle) const
	{
		// (seen driving: a tracking shot. Riding on her hull the meadow only slides by: the third cut held 8.5 s from a
		// tank over an empty meadow)
		if (Shot != Kind::Track || Subject < 0) return false;
		const Track& Tr = Rec->Tracks[static_cast<size_t>(Subject)];
		return Tr.IsVehicle() && (Tr.Fell < 0.0 || Tr.Fell > Battle) && LandFeed::SpotAt(Tr, Battle).V > 0.5;
	}

	Vec Director::KeepAbove(Vec At, double Least) const
	{
		// The ground under the eye and round it (a slope rising just ahead of the lens), the nearer the more: a bank's
		// top 1.5 m off lifts the lens a little, one beside it all the way, so the lens rises smoothly as it nears a bank
		// (Settings::Climb a metre), not a third of a metre in a frame as a bank's edge came within 0.6 m (the pan's
		// kicks in the cloud test: the second cut's low lens popping over a bank's top).
		double Ground = HeightAt(At.X, At.Y);
		for (int K = 0; K < 8; ++K)
		{
			const double A = K * Pi * 0.25;
			const double Cx = std::cos(A), Sy = std::sin(A);
			for (const double R : { 0.3, 0.6, 0.9, 1.2, 1.5, 1.8 })
				Ground = std::max(Ground, HeightAt(At.X + Cx * R, At.Y + Sy * R) - Set.Climb * R);
		}
		At.Z = std::max(At.Z, Ground + Least);
		return At;
	}

	Vec Director::KeepAboveAhead(Vec At, const Vec& Toward, double Least) const
	{
		At = KeepAbove(At, Least);
		Vec Way = Toward - At;
		Way.Z = 0.0;
		const double Gap = Way.Length();
		if (Gap < 0.05) return At;
		const Vec Dir = Way * (1.0 / Gap);
		const double Long = std::min(Gap + 0.6, 3.6);
		for (double D = 0.3; D <= Long + 1e-9; D += 0.3)
		{
			const Vec P = At + Dir * D;
			At.Z = std::max(At.Z, HeightAt(P.X, P.Y) + Least - Set.Climb * D);
		}
		return At;
	}

	double Director::CropAt(double X, double Y) const
	{
		if (!Here.Crop) return 0.0;
		const double C = Here.Crop(X, Y);
		return C == C ? std::clamp(C, 0.0, 3.0) : 0.0;
	}

	bool Director::Begin(const LandFeed::Recording& InRecording, const World& InWorld, const Settings& InSettings)
	{
		Rec = &InRecording;
		Here = InWorld;
		Set = InSettings;
		Dice = Set.Seed * 0xD1B54A32D192ED03ull + 7;
		for (double& P : Phase) P = Roll() * 2.0 * Pi;
		Shot = Kind::Open;
		Subject = -1;
		Lift = LiftSwing = 0.0;
		CornHeld = -1e9;
		CornSwing = LookLiftSwing = 0.0;
		ForeHeld = -1e9;
		ShotStarted = 0.0;
		ShotLength = Set.Opening;
		CutCount = 0;
		CutLog.clear();
		Recent.clear();
		Appointed.clear();
		LeftWhy = Why::Start;
		Pending = -1;
		LastAction = 0.0;
		LastBattle = -1.0;
		RoundFrom = 0;
		NextLook = 0.0;
		bLookAhead = false;
		bShoveCut = false;
		ScreenedSince = -1.0;
		BadSince = -1.0;
		bClosed = false;
		bMassNow = bMassDone = false;
		MassCutAt = -1.0;
		bStretch = false;
		RideBlock = -1;
		LastFallCut = -100.0;
		bNearWatch = false;
		bFallEye = false;
		LastTankSide = -1;
		LeanBlock = -1;
		LeanUntil = -1.0;
		Beats.clear();
		LastRadioReal = -1e9;
		bFocusShot = false;
		FocusEased = FocusSwing = 0.0;
		Now.Focus = 0.0;
		bRadioChoice = false;
		HeroLooked = -1.0;
		Index();
		Nerves = NervesWant = 0.0;
		SinceSquad = 0;
		UsedSpots.clear();
		DarkSpots.clear();
		DarkSince = LightSince = -1.0;
		DarkLeft = DarkRunNow = 0;
		bAskedUnfit = false;
		bAsked = bAskedBuilding = bShotAsked = false;
		Houses.clear();
		bHousesFound = false;
		AskTries.clear();
		TryHouse = ShotHouse = -1;
		AskAgain = 0;
		AskLeft.clear();
		BurstCutAt = -1.0;
		LostSince = -1.0;
		LookSwing = Vec();
		UpJitter = 0.0;
		Pace = 1.0;
		bPaced = false;
		LookLift = 0.0;
		OwnPush = Vec();
		PushSwing = Vec();
		WaitFor = -1.0;
		bChoosing = false;
		bScanning = false;
		bTrying = false;
		Options.clear();
		ChoiceFramesMost = 0;
		Place(Kind::Open, -1, 0.0);
		EyeEased = Stand;
		LookEased = Target;
		Now.Eye = Stand;
		Now.Look = Target;
		Now.bCut = true;
		return Rec != nullptr;
	}

	void Director::Index()
	{
		// The lists the film looks things up in, made from the recording as it stands: a battle read whole once, a live one
		// (the general's) afresh when its men are placed again, and kept up as it grows (KeepUp).
		ShownAt.assign(Rec->Tracks.size(), -1e9);
		ShooterOf.clear();
		TrackKinds.clear();
		HiddenStamp.clear();
		HiddenVal.clear();
		LeadMen.clear();
		LeadMenTracks = 0;
		LeadBlockNow = -1;
		LeadLookAt = -1e9;
		Radios.clear();
		RadioNext = 0;
		RadioStage = 0;
		RadioNow = -1;
		bRadiosFound = false;
		for (size_t I = 0; I < Rec->Tracks.size(); ++I)
		{
			const Track& Tr = Rec->Tracks[I];
			if (!Tr.bTwin) ShooterOf.emplace_back((static_cast<int64_t>(Tr.Block) * 65536 + Tr.K) * 256 + Tr.J, static_cast<int>(I));
		}
		std::sort(ShooterOf.begin(), ShooterOf.end());
		Heavy.clear();
		for (size_t I = 0; I < Rec->Rounds.size(); ++I)
		{
			const std::string& W = Rec->Rounds[I].W;
			if (W == "mortar" || W == "gun" || W == "tank" || W == "at" || W == "bomb") Heavy.push_back(static_cast<int>(I));
		}
		std::sort(Heavy.begin(), Heavy.end(), [this](int A, int B) { return Rec->Rounds[static_cast<size_t>(A)].L < Rec->Rounds[static_cast<size_t>(B)].L; });
		Burnt.clear();
		Vehicles.clear();
		for (size_t I = 0; I < Rec->Tracks.size(); ++I)
		{
			const Track& Tr = Rec->Tracks[I];
			if (Tr.bTwin || !(Tr.Role == "tank" || Tr.Role == "lorry")) continue;
			if (Tr.Fell >= 0.0) Burnt.push_back(static_cast<int>(I));
			else Vehicles.push_back(static_cast<int>(I));
		}
		std::sort(Burnt.begin(), Burnt.end(), [this](int A, int B) { return Rec->Tracks[static_cast<size_t>(A)].Fell < Rec->Tracks[static_cast<size_t>(B)].Fell; });
		ByLanding.clear();
		ByLandingRounds = 0;
		IndexLandings();
		BlockIx.clear();
		BlockIxTracks = BlockIxBlocks = 0;
		MenT = -1e18;
		MenTracks = 0;
		// (the fastest a man goes between two of his points, and on past his last at that speed)
		MenFastest = 0.0;
		FastSeen.assign(Rec->Tracks.size(), 0);
		for (size_t I = 0; I < Rec->Tracks.size(); ++I) Faster(I);
		KeptTracks = Rec->Tracks.size();
		KeptRounds = Rec->Rounds.size();
		// The film's own section: from the whole battle (the recording is the script); a live one's is chosen once the
		// fight has run a while (KeepUp).
		if (!Rec->bLive) ChooseHero(Rec->End, -1.0);
		else HeroBlock = HeroK = -1;
	}

	void Director::Faster(size_t I)
	{
		// (the fastest a man goes between two of his points, and on past his last at that speed: his points since last
		// asked, the last again where a live battle's provisional last point was moved since)
		const Track& Tr = Rec->Tracks[I];
		size_t& Seen = FastSeen[I];
		if (Tr.bTwin || !Tr.IsMan()) { Seen = Tr.At.size(); return; }
		for (size_t K = std::max<size_t>(1, Seen > 0 ? Seen - 1 : 0); K < Tr.At.size(); ++K)
		{
			const double Dt = std::max(1e-6, Tr.At[K].T - Tr.At[K - 1].T);
			MenFastest = std::max(MenFastest, std::hypot(Tr.At[K].X - Tr.At[K - 1].X, Tr.At[K].Y - Tr.At[K - 1].Y) / Dt);
		}
		Seen = Tr.At.size();
	}

	void Director::KeepUp(double Battle)
	{
		// A live battle grows under the film as the rules fight it (LandFeed::AppendDelta): its new tracks, rounds and
		// knocked-out vehicles are put into the lists as they come; the men placed again (the setup step) start them
		// afresh. A recording read whole never changes, and is never asked.
		const size_t Tracks = Rec->Tracks.size(), Rounds = Rec->Rounds.size();
		if (Tracks < KeptTracks || Rounds < KeptRounds)
		{
			Index();
			if (Subject >= static_cast<int>(Tracks)) Subject = -1;
			bCutAsked = true;
			Pending = -1;
			Appointed.clear();
			RoundFrom = 0;
			return;
		}
		if (Tracks > KeptTracks)
		{
			ShownAt.resize(Tracks, -1e9);
			FastSeen.resize(Tracks, 0);
			for (size_t I = KeptTracks; I < Tracks; ++I)
			{
				const Track& Tr = Rec->Tracks[I];
				if (!Tr.bTwin) ShooterOf.emplace_back((static_cast<int64_t>(Tr.Block) * 65536 + Tr.K) * 256 + Tr.J, static_cast<int>(I));
				if (!Tr.bTwin && (Tr.Role == "tank" || Tr.Role == "lorry")) Vehicles.push_back(static_cast<int>(I));
			}
			std::sort(ShooterOf.begin(), ShooterOf.end());
			KeptTracks = Tracks;
		}
		for (size_t I = 0; I < Tracks; ++I)
			if (Rec->Tracks[I].At.size() != FastSeen[I]) Faster(I);
		if (Rounds > KeptRounds)
		{
			const size_t Was = Heavy.size();
			for (size_t I = KeptRounds; I < Rounds; ++I)
			{
				const std::string& W = Rec->Rounds[I].W;
				if (W == "mortar" || W == "gun" || W == "tank" || W == "at" || W == "bomb") Heavy.push_back(static_cast<int>(I));
			}
			auto ByLand = [this](int A, int B) { return Rec->Rounds[static_cast<size_t>(A)].L < Rec->Rounds[static_cast<size_t>(B)].L; };
			std::sort(Heavy.begin() + static_cast<std::ptrdiff_t>(Was), Heavy.end(), ByLand);
			std::inplace_merge(Heavy.begin(), Heavy.begin() + static_cast<std::ptrdiff_t>(Was), Heavy.end(), ByLand);
			KeptRounds = Rounds;
		}
		// (a vehicle knocked out since: its fuel ball)
		for (size_t V = 0; V < Vehicles.size();)
		{
			const int I = Vehicles[V];
			const double Fell = Rec->Tracks[static_cast<size_t>(I)].Fell;
			if (Fell < 0.0) { ++V; continue; }
			auto It = std::upper_bound(Burnt.begin(), Burnt.end(), Fell, [this](double T, int K) { return T < Rec->Tracks[static_cast<size_t>(K)].Fell; });
			Burnt.insert(It, I);
			Vehicles[V] = Vehicles.back();
			Vehicles.pop_back();
		}
		// The film's own section, once the fight has run a while on what has been fought so far; and again when its last
		// man is down or gone (a live battle has no script to choose the section that lasts).
		if (Rec->Contact >= 0.0 && Rec->Fought >= Rec->Contact + Set.HeroAfter && Battle - HeroLooked >= 1.0)
		{
			HeroLooked = Battle;
			bool bStands = false;
			for (size_t I = 0; I < Tracks && HeroBlock >= 0 && !bStands; ++I)
			{
				const Track& Tr = Rec->Tracks[I];
				bStands = InSquad(static_cast<int>(I)) && !(Tr.Fell >= 0.0 && Tr.Fell <= Battle) && !(Tr.Out >= 0.0 && Tr.Out <= Battle);
			}
			if (!bStands) ChooseHero(Rec->Fought, Battle);
		}
	}

	void Director::ChooseHero(double Until, double StandingAt)
	{
		// Of the own side's rifle sections of four men or more, the one that fires most and lasts longest in the fight up
		// to Until (with StandingAt, of its men still up and on the field then: a section wiped out is not chosen again).
		HeroBlock = HeroK = -1;
		{
			const double Contact = Rec->Contact >= 0.0 ? Rec->Contact : 0.0;
			const double Finish = std::max(Until, Contact + 1.0);
			std::map<std::pair<int, int>, std::pair<double, int>> Squads;
			for (const Track& Tr : Rec->Tracks)
			{
				if (Tr.bTwin || !Tr.IsMan()) continue;
				const LandFeed::Block* B = Rec->BlockOf(Tr.Block);
				if (!B || B->Side != Set.Own || !B->Team.empty()) continue;
				if (StandingAt >= 0.0 && ((Tr.Fell >= 0.0 && Tr.Fell <= StandingAt) || (Tr.Out >= 0.0 && Tr.Out <= StandingAt) || Tr.Start() > StandingAt)) continue;
				const double Upto = Tr.Fell >= 0.0 ? Tr.Fell : (Tr.Out >= 0.0 ? Tr.Out : Finish);
				const double Lasted = std::clamp((Upto - Contact) / (Finish - Contact), 0.0, 1.0);
				int Fired = 0;
				for (const double T : Tr.Shots) Fired += T >= Contact ? 1 : 0;
				std::pair<double, int>& Sq = Squads[{ Tr.Block, Tr.K }];
				Sq.first += 10.0 * Lasted + 0.25 * std::min(Fired, 40);
				++Sq.second;
			}
			double Best = -1.0;
			for (const auto& [Key, Sq] : Squads)
			{
				if (Sq.second < 4 || Sq.first <= Best) continue;
				Best = Sq.first;
				HeroBlock = Key.first;
				HeroK = Key.second;
			}
		}
	}

	void Director::Ask(Kind K, int InSubject, bool bBuilding)
	{
		AskAgain = 0;
		AskLeft.clear();
		bAsked = true;
		AskedKind = K;
		AskedSubject = InSubject;
		bAskedBuilding = bBuilding;
		bCutAsked = true;
	}

	void Director::Ride(int Block)
	{
		// (the player's own pick: the platoon cam lets go)
		LeadSide = -1;
		LeadBlockNow = -1;
		RideWith(Block);
	}

	void Director::RideWith(int Block)
	{
		if (Block == RideBlock) return;
		RideBlock = Block < 0 ? -1 : Block;
		// (to them at once, unless the shot on the screen is already on one of them; let go, the shot holds its course)
		if (RideBlock >= 0 && Rec && (Subject < 0 || OffRide(Subject))) bCutAsked = true;
	}

	void Director::FollowLead(int Army)
	{
		if (Army == LeadSide) return;
		// (let go: the platoon ridden for the lead is let go too)
		if (Army < 0 && LeadBlockNow >= 0 && RideBlock == LeadBlockNow) RideWith(-1);
		LeadSide = Army < 0 ? -1 : Army;
		LeadBlockNow = -1;
		LeadLookAt = -1e9;
	}

	bool Director::InLead(size_t TrackIndex, double Battle) const
	{
		for (const Track::Span& Sp : Rec->Tracks[TrackIndex].Lead)
			if (Battle >= Sp.From && Battle <= Sp.To) return true;
		return false;
	}

	bool Director::InRush(size_t TrackIndex, double Battle) const
	{
		for (const Track::Span& Sp : Rec->Tracks[TrackIndex].Element)
			if (Sp.bMove && Battle >= Sp.From && Battle <= Sp.To) return true;
		return false;
	}

	void Director::LookForLead(double Battle, double Real)
	{
		if (LeadSide < 0 || !Rec) return;
		// (the ride let go of itself, none of the platoon standing or seen: the next to lead is looked for at once)
		if (LeadBlockNow >= 0 && RideBlock != LeadBlockNow)
		{
			LeadBlockNow = -1;
			LeadLookAt = -1e9;
		}
		if (Real < LeadLookAt) return;
		LeadLookAt = Real + 0.5;
		while (LeadMenTracks < Rec->Tracks.size())
		{
			if (!Rec->Tracks[LeadMenTracks].Lead.empty()) LeadMen.push_back(static_cast<int>(LeadMenTracks));
			++LeadMenTracks;
		}
		// (the platoon with the most of its side's leading men up and seen now; on a tie, the one ridden)
		std::vector<std::pair<int, int>> Count;
		for (const int I : LeadMen)
		{
			const Track& Tr = Rec->Tracks[static_cast<size_t>(I)];
			if (Tr.bTwin || (Tr.Fell >= 0.0 && Tr.Fell <= Battle) || (Tr.Out >= 0.0 && Tr.Out <= Battle)) continue;
			if (!InLead(static_cast<size_t>(I), Battle)) continue;
			const LandFeed::Block* B = BlockOfTrack(I);
			if (!B || B->Side != LeadSide || HiddenNow(I)) continue;
			bool bFound = false;
			for (std::pair<int, int>& C : Count)
				if (C.first == Tr.Block) { ++C.second; bFound = true; break; }
			if (!bFound) Count.push_back({ Tr.Block, 1 });
		}
		int Best = -1, BestN = 0;
		for (const std::pair<int, int>& C : Count)
			if (C.second > BestN || (C.second == BestN && C.first == LeadBlockNow)) { Best = C.first; BestN = C.second; }
		if (Best >= 0 && Best != LeadBlockNow)
		{
			LeadBlockNow = Best;
			RideWith(Best);
		}
	}

	void Director::FindHouses()
	{
		bHousesFound = true;
		Houses.clear();
		if (!Here.Free || !Rec) return;
		// (a house's floor or a wall's line is no place for the eye at any height; a wall's line is a few tenths of a metre
		// across, so a point is a house's only if the eye may not stand 1.5 m (then 0.9 m) round it either)
		auto Built = [this](double X, double Y) { return !Here.Free(Vec(X, Y, 1.0e4)); };
		auto Inside = [&Built](double X, double Y, double By) { return Built(X, Y) && Built(X + By, Y) && Built(X - By, Y) && Built(X, Y + By) && Built(X, Y - By); };
		const double W = Rec->Ground.W > 0 ? Rec->Ground.W : 3000.0, H = Rec->Ground.H > 0 ? Rec->Ground.H : 2000.0;
		constexpr double Coarse = 6.0, Fine = 2.0;
		const int Nx = static_cast<int>(W / Coarse) + 1, Ny = static_cast<int>(H / Coarse) + 1;
		std::vector<uint8_t> Hit(static_cast<size_t>(Nx) * static_cast<size_t>(Ny), 0);
		for (int J = 0; J < Ny; ++J)
			for (int I = 0; I < Nx; ++I)
				if (Inside(I * Coarse, J * Coarse, 1.5)) Hit[static_cast<size_t>(J) * Nx + I] = 1;
		// (the hits joined to their neighbours: a house, or a row of them along a street, one each)
		std::vector<int> Stack;
		for (int J0 = 0; J0 < Ny; ++J0)
			for (int I0 = 0; I0 < Nx; ++I0)
			{
				if (Hit[static_cast<size_t>(J0) * Nx + I0] != 1) continue;
				int I1 = I0, I2 = I0, J1 = J0, J2 = J0;
				Stack.assign(1, J0 * Nx + I0);
				Hit[static_cast<size_t>(J0) * Nx + I0] = 2;
				while (!Stack.empty())
				{
					const int At = Stack.back();
					Stack.pop_back();
					const int I = At % Nx, J = At / Nx;
					I1 = std::min(I1, I); I2 = std::max(I2, I); J1 = std::min(J1, J); J2 = std::max(J2, J);
					for (int Dj = -1; Dj <= 1; ++Dj)
						for (int Di = -1; Di <= 1; ++Di)
						{
							const int Ii = I + Di, Jj = J + Dj;
							if (Ii < 0 || Jj < 0 || Ii >= Nx || Jj >= Ny || Hit[static_cast<size_t>(Jj) * Nx + Ii] != 1) continue;
							Hit[static_cast<size_t>(Jj) * Nx + Ii] = 2;
							Stack.push_back(Jj * Nx + Ii);
						}
				}
				House Ho;
				double Sx = 0.0, Sy = 0.0;
				for (double Y = J1 * Coarse - 6.0; Y <= J2 * Coarse + 6.0; Y += Fine)
					for (double X = I1 * Coarse - 6.0; X <= I2 * Coarse + 6.0; X += Fine)
						if (Inside(X, Y, 0.9))
						{
							Ho.Inside.push_back(Vec(X, Y, HeightAt(X, Y) + 3.0));
							Sx += X;
							Sy += Y;
						}
				if (Ho.Inside.empty()) continue;
				Ho.C = Vec(Sx / Ho.Inside.size(), Sy / Ho.Inside.size(), 0.0);
				Ho.C.Z = HeightAt(Ho.C.X, Ho.C.Y) + 3.0;
				for (const Vec& P : Ho.Inside) Ho.R = std::max(Ho.R, Flat(P, Ho.C) + 1.0);
				Houses.push_back(std::move(Ho));
			}
	}

	bool Director::HouseSeenIn(const Vec& Eye, const Vec& Look, double Fov, int H) const
	{
		if (H < 0 || static_cast<size_t>(H) >= Houses.size()) return false;
		const House& Ho = Houses[static_cast<size_t>(H)];
		// (big enough: its half-width (at most 8 m of a row of them) over its distance, against the picture's half-height)
		const double Far = std::max(1.0, Flat(Eye, Ho.C));
		if (std::min(Ho.R, 8.0) / Far < Set.HouseLeast * std::tan(Fov * 0.5 * Pi / 180.0)) return false;
		Pose P;
		P.Eye = Eye;
		P.Look = Look;
		P.Fov = Fov;
		// (its points in the picture, a dozen at most asked, seen from the eye: the line to one stops 2.5 m short, so a
		// point just inside the wall facing the eye is seen and one behind it is not)
		const size_t Every = std::max<size_t>(1, Ho.Inside.size() / 12);
		int Seen = 0;
		for (size_t K = 0; K < Ho.Inside.size(); K += Every)
		{
			const Vec& Pt = Ho.Inside[K];
			if (!InPicture(P, Pt, 0.9)) continue;
			if (!SeenFrom(Eye, Pt, -1)) continue;
			if (Here.Open && !Here.Open(Eye, Pt)) continue;
			if (++Seen >= Set.HouseSeen) return true;
		}
		return false;
	}

	void Director::MakeAskTries(double Battle)
	{
		AskTries.clear();
		AskNext = 1;
		AskNextV = 0;
		if (!Rec) return;
		const bool bAskedValid = AskedSubject >= 0 && static_cast<size_t>(AskedSubject) < Rec->Tracks.size();
		const bool bManAsked = bAskedValid && Rec->Tracks[static_cast<size_t>(AskedSubject)].IsMan();
		const bool bTankAsked = bAskedValid && AskedKind == Kind::Tank && Rec->Tracks[static_cast<size_t>(AskedSubject)].Role == "tank";
		const bool bOthers = (bManAsked && (AskedKind == Kind::Shoulder || AskedKind == Kind::Low)) || bTankAsked;
		if (!bAskedBuilding && !bOthers) return;
		const LandFeed::Block* Asked = bAskedValid ? BlockOfTrack(AskedSubject) : nullptr;
		const int AskedSide = Asked ? Asked->Side : Set.Own;
		const Track* AskedTr = bManAsked || bTankAsked ? &Rec->Tracks[static_cast<size_t>(AskedSubject)] : nullptr;
		const bool bFallenAsked = AskedTr && AskedTr->Fell >= 0.0 && AskedTr->Fell <= Battle;
		const LandFeed::Spot AskedAt = AskedTr ? LandFeed::SpotAt(*AskedTr, Battle) : LandFeed::Spot();
		// (a man the shot may be of: up, through the next 4 s, or fallen as the asked man is; not in a lorry's bed, not
		// hidden from the player, not the mortars' or an observer far back)
		auto May = [&](size_t I) -> bool
		{
			const Track& Tr = Rec->Tracks[I];
			if (Tr.bTwin || Tr.Start() > Battle || (Tr.Out >= 0.0 && Tr.Out < Battle + Soon(4.0))) return false;
			if (bTankAsked ? Tr.Role != "tank" : !Tr.IsMan()) return false;
			if (Tr.Role == "mortar" || Tr.Role == "fo") return false;
			const bool bFallen = Tr.Fell >= 0.0 && Tr.Fell <= Battle;
			if (bFallen != bFallenAsked) return false;
			if (!bFallen && Tr.Fell >= 0.0 && Tr.Fell < Battle + Soon(4.0)) return false;
			const LandFeed::Change* C = LandFeed::ChangeAt(Tr, Battle);
			if (C && C->Pose == "mounted") return false;
			return !HiddenNow(static_cast<int>(I)) && !OffRide(static_cast<int>(I));
		};
		struct Scored
		{
			double Score;
			AskTry Try;
		};
		std::vector<Scored> All;
		if (bAskedBuilding)
		{
			if (!bHousesFound) FindHouses();
			if (Houses.empty()) return;
			for (size_t I = 0; I < Rec->Tracks.size(); ++I)
			{
				if (!May(I)) continue;
				const LandFeed::Spot S = LandFeed::SpotAt(Rec->Tracks[I], Battle);
				double Gap = Set.HouseFar;
				AskTry T;
				T.Track = static_cast<int>(I);
				for (size_t H = 0; H < Houses.size(); ++H)
				{
					if (std::hypot(Houses[H].C.X - S.X, Houses[H].C.Y - S.Y) > Gap + Houses[H].R) continue;
					for (const Vec& P : Houses[H].Inside)
					{
						const double D = std::hypot(P.X - S.X, P.Y - S.Y);
						if (D < Gap) { Gap = D; T.House = static_cast<int>(H); T.Aim = P; }
					}
				}
				if (T.House < 0) continue;
				const LandFeed::Block* B = BlockOfTrack(T.Track);
				// (near a house first, the asked man's side first, the asked man himself first of all)
				const double Score = (Gap > Set.HouseNear ? 2000.0 : 0.0) + (B && B->Side != AskedSide ? 1000.0 : 0.0) + (T.Track == AskedSubject ? -500.0 : 0.0) + Gap;
				All.push_back({ Score, T });
			}
		}
		else
		{
			for (size_t I = 0; I < Rec->Tracks.size(); ++I)
			{
				if (static_cast<int>(I) != AskedSubject && !May(I)) continue;
				const LandFeed::Block* B = BlockOfTrack(static_cast<int>(I));
				if (!B || B->Side != AskedSide) continue;
				const LandFeed::Spot S = LandFeed::SpotAt(Rec->Tracks[I], Battle);
				const double D = std::hypot(S.X - AskedAt.X, S.Y - AskedAt.Y);
				if (D > Set.AskWithin) continue;
				AskTry T;
				T.Track = static_cast<int>(I);
				// (his own unit's men first, the nearest first)
				All.push_back({ static_cast<int>(I) == AskedSubject ? -1.0 : D + (Rec->Tracks[I].Block != AskedTr->Block ? Set.AskWithin : 0.0), T });
			}
		}
		std::sort(All.begin(), All.end(), [](const Scored& A, const Scored& B) { return A.Score < B.Score || (A.Score == B.Score && A.Try.Track < B.Try.Track); });
		// (spread over the field, each AskApart from those before it: the nearest dozen of the asked officer's men at
		// Normandy's 148 s were all under the same barrage)
		std::vector<LandFeed::Spot> Taken;
		for (size_t K = 0; K < All.size() && static_cast<int>(AskTries.size()) <= Set.AskOthers; ++K)
		{
			const LandFeed::Spot S = LandFeed::SpotAt(Rec->Tracks[static_cast<size_t>(All[K].Try.Track)], Battle);
			bool bNear = false;
			for (const LandFeed::Spot& T : Taken) bNear = bNear || std::hypot(S.X - T.X, S.Y - T.Y) < Set.AskApart;
			if (bNear && K > 0) continue;
			Taken.push_back(S);
			AskTries.push_back(All[K].Try);
		}
	}

	bool Director::MassStretch(Vec& OutAt, Vec& OutAlong, double& OutFrom, double& OutTo) const
	{
		if (!bStretch) return false;
		OutAt = StretchAt;
		OutAlong = StretchAlong;
		OutFrom = StretchFrom;
		OutTo = StretchTo;
		return true;
	}

	void Director::Lean(int Block)
	{
		LeanBlock = Block < 0 ? -1 : Block;
		LeanUntil = -1.0;
	}

	bool Director::RideStands(double Battle) const
	{
		for (size_t I = 0; I < Rec->Tracks.size(); ++I)
		{
			const Track& Tr = Rec->Tracks[I];
			if (Tr.Block != RideBlock || Tr.bTwin || Tr.Start() > Battle || (Tr.Fell >= 0.0 && Tr.Fell <= Battle) || (Tr.Out >= 0.0 && Tr.Out <= Battle)) continue;
			if (Tr.Role == "lorry" || Tr.Role == "gun") continue;
			return !HiddenNow(static_cast<int>(I));
		}
		return false;
	}

	namespace
	{
		size_t BucketOf(int64_t I, int64_t J, size_t Count)
		{
			const uint64_t H = static_cast<uint64_t>(I) * 0x9E3779B97F4A7C15ull ^ static_cast<uint64_t>(J) * 0xC2B2AE3D27D4EB4Full;
			return static_cast<size_t>(H ^ (H >> 29)) & (Count - 1);
		}
		constexpr double MenSquare = 16.0, HeatSquare = 35.0;
		constexpr size_t MenBuckets = size_t(1) << 12, HeatBuckets = size_t(1) << 10;
	}

	const LandFeed::Block* Director::BlockOfTrack(int TrackIndex) const
	{
		if (!Rec || TrackIndex < 0 || static_cast<size_t>(TrackIndex) >= Rec->Tracks.size()) return nullptr;
		if (BlockIxTracks != Rec->Tracks.size() || BlockIxBlocks != Rec->Blocks.size())
		{
			BlockIxTracks = Rec->Tracks.size();
			BlockIxBlocks = Rec->Blocks.size();
			BlockIx.assign(Rec->Tracks.size(), -1);
			for (size_t I = 0; I < Rec->Tracks.size(); ++I)
			{
				const LandFeed::Block* B = Rec->BlockOf(Rec->Tracks[I].Block);
				BlockIx[I] = B ? static_cast<int>(B - Rec->Blocks.data()) : -1;
			}
		}
		const int B = BlockIx[static_cast<size_t>(TrackIndex)];
		return B >= 0 ? &Rec->Blocks[static_cast<size_t>(B)] : nullptr;
	}

	uint8_t Director::KindOf(size_t TrackIndex) const
	{
		// (grown with a live battle's new tracks; a track's role is set when it is made)
		while (TrackKinds.size() < Rec->Tracks.size())
		{
			const std::string& Role = Rec->Tracks[TrackKinds.size()].Role;
			TrackKinds.push_back(Role == "tank" ? KindTank : Role == "gun" ? KindGun : Role == "lorry" ? KindLorry : 0);
		}
		return TrackKinds[TrackIndex];
	}

	void Director::PlaceMen(double T) const
	{
		if (MenHead.empty()) MenHead.assign(MenBuckets, -1);
		for (const uint32_t B : MenUsed) MenHead[B] = -1;
		MenUsed.clear();
		MenAt.clear();
		MenNext.clear();
		MenT = T;
		MenTracks = Rec->Tracks.size();
		RaySteps += static_cast<long>(Rec->Tracks.size() / 8);
		for (size_t I = 0; I < Rec->Tracks.size(); ++I)
		{
			const Track& Tr = Rec->Tracks[I];
			if (Tr.bTwin || IsVehicleAt(I)) continue;
			const LandFeed::Spot S = LandFeed::SpotAt(Tr, T);
			const size_t B = BucketOf(static_cast<int64_t>(std::floor(S.X / MenSquare)), static_cast<int64_t>(std::floor(S.Y / MenSquare)), MenBuckets);
			if (MenHead[B] < 0) MenUsed.push_back(static_cast<uint32_t>(B));
			MenNext.push_back(MenHead[B]);
			MenHead[B] = static_cast<int32_t>(MenAt.size());
			MenAt.push_back({ static_cast<int>(I), S.X, S.Y });
		}
	}

	void Director::IndexRadios()
	{
		// The fresh calls for fire, found once: a forward observer's call with its spot (the tactics thread's cl [t, x, y]),
		// none of his guns' rounds landing from RadioQuiet before it to RadioSoonest after (a correction to a mission
		// already firing is not a new one), and his guns' first salvo landing near the spot RadioSoonest to RadioLatest
		// after it, RadioRounds or more of them in the 15 s from then.
		if (bRadiosFound || !Rec || Rec->bLive || Set.RadioEvery <= 0.0) return;
		bRadiosFound = true;
		IndexLandings();
		auto LandingFrom = [this](double V)
		{
			return std::lower_bound(ByLanding.begin(), ByLanding.end(), V, [this](int K, double X) { return Rec->Rounds[static_cast<size_t>(K)].L < X; });
		};
		std::vector<int> Near;
		for (size_t I = 0; I < Rec->Tracks.size(); ++I)
		{
			const Track& Tr = Rec->Tracks[I];
			if (Tr.Calls.empty() || Tr.bTwin) continue;
			for (const Track::Call& C : Tr.Calls)
			{
				if (!C.bSpot) continue;
				bool bQuiet = true;
				for (auto It = LandingFrom(C.T - Set.RadioQuiet); It != ByLanding.end() && bQuiet; ++It)
				{
					const LandFeed::Round& R = Rec->Rounds[static_cast<size_t>(*It)];
					if (R.L >= C.T + Set.RadioSoonest) break;
					bQuiet = R.By != Tr.Block;
				}
				if (!bQuiet) continue;
				Near.clear();
				for (auto It = LandingFrom(C.T + Set.RadioSoonest); It != ByLanding.end(); ++It)
				{
					const LandFeed::Round& R = Rec->Rounds[static_cast<size_t>(*It)];
					if (R.L > C.T + Set.RadioLatest + 15.0) break;
					if (R.By == Tr.Block && std::hypot(R.X - C.X, R.Y - C.Y) <= Set.RadioNear) Near.push_back(*It);
				}
				// (the first salvo: the first of them with another landing within 1.2 s, else the first alone)
				size_t First = Near.size();
				for (size_t K = 0; K < Near.size(); ++K)
				{
					const double L = Rec->Rounds[static_cast<size_t>(Near[K])].L;
					if (L > C.T + Set.RadioLatest) break;
					if (First == Near.size()) First = K;
					if (K + 1 < Near.size() && Rec->Rounds[static_cast<size_t>(Near[K + 1])].L - L <= 1.2)
					{
						First = K;
						break;
					}
				}
				if (First == Near.size()) continue;
				Radio Rd;
				Rd.T = C.T;
				Rd.Land = Rec->Rounds[static_cast<size_t>(Near[First])].L;
				Rd.Caller = static_cast<int>(I);
				int Count = 0, InSalvo = 0;
				for (size_t K = First; K < Near.size(); ++K)
				{
					const LandFeed::Round& R = Rec->Rounds[static_cast<size_t>(Near[K])];
					if (R.L > Rd.Land + 15.0) break;
					++Count;
					if (R.L > Rd.Land + 1.2) continue;
					Rd.X += R.X;
					Rd.Y += R.Y;
					++InSalvo;
				}
				if (Count < Set.RadioRounds) continue;
				Rd.X /= InSalvo;
				Rd.Y /= InSalvo;
				Radios.push_back(Rd);
			}
		}
		std::stable_sort(Radios.begin(), Radios.end(), [](const Radio& A, const Radio& B) { return A.T < B.T; });
		RadioNext = 0;
	}

	int Director::RadioMen(const Radio& Rd, double Battle, int Out[4]) const
	{
		// (the men the barrage's cut may go to, framed with the salvo beyond them: the two of the other side nearest its
		// middle, under it; then the two of the caller's own side nearest it, 40 to 600 m off, watching it from their line,
		// as a war film shows the shells landing on the hedge ahead. Normandy's men under the fire are dug in behind the
		// hedges, unseen from anywhere a camera stands)
		const LandFeed::Block* Bc = BlockOfTrack(Rd.Caller);
		const int OwnSide = Bc ? Bc->Side : -1;
		int Under[2] = { -1, -1 }, Watch[2] = { -1, -1 };
		double UnderGap[2] = { Set.RadioNear * 1.5, Set.RadioNear * 1.5 }, WatchGap[2] = { 600.0, 600.0 };
		for (size_t I = 0; I < Rec->Tracks.size(); ++I)
		{
			const Track& Tr = Rec->Tracks[I];
			// (not one going down before the salvo has landed and been seen: his fall would take the picture with him)
			if (Tr.bTwin || KindOf(I) != 0 || Tr.Start() > Battle || (Tr.Fell >= 0.0 && Tr.Fell < Rd.Land + Soon(1.5)) || (Tr.Out >= 0.0 && Tr.Out < Rd.Land + Soon(4.0))) continue;
			const LandFeed::Block* B = BlockOfTrack(static_cast<int>(I));
			if (!B) continue;
			const bool bOwn = B->Side == OwnSide;
			const LandFeed::Spot S = LandFeed::SpotAt(Tr, Battle);
			const double Gap = std::hypot(S.X - Rd.X, S.Y - Rd.Y);
			double* Gaps = bOwn ? WatchGap : UnderGap;
			int* Men = bOwn ? Watch : Under;
			if (Gap >= Gaps[1] || (bOwn && Gap < 40.0)) continue;
			if (HiddenNow(static_cast<int>(I)) || OffRide(static_cast<int>(I))) continue;
			const LandFeed::Change* C = LandFeed::ChangeAt(Tr, Battle);
			if (C && C->Pose == "mounted") continue;
			const int K = Gap < Gaps[0] ? 0 : 1;
			if (K == 0)
			{
				Gaps[1] = Gaps[0];
				Men[1] = Men[0];
			}
			Gaps[K] = Gap;
			Men[K] = static_cast<int>(I);
		}
		RaySteps += static_cast<long>(Rec->Tracks.size() / 16);
		int N = 0;
		// (in turn, one under it and one watching: a cut has sixteen tries, and in the hedges the men under it are seldom seen)
		for (const int M : { Under[0], Watch[0], Under[1], Watch[1] })
			if (M >= 0) Out[N++] = M;
		return N;
	}

	void Director::IndexLandings()
	{
		if (!Rec || ByLandingRounds == Rec->Rounds.size()) return;
		// (the rounds come in firing order; those new since are put in by when they land)
		for (size_t I = ByLandingRounds; I < Rec->Rounds.size(); ++I)
		{
			const double L = Rec->Rounds[I].L;
			auto It = std::upper_bound(ByLanding.begin(), ByLanding.end(), L, [this](double V, int K) { return V < Rec->Rounds[static_cast<size_t>(K)].L; });
			ByLanding.insert(It, static_cast<int>(I));
		}
		ByLandingRounds = Rec->Rounds.size();
	}

	void Director::Gather(double From, double To)
	{
		Ahead.clear();
		// (the rounds landing From to To, by the index of landings, not every round fired in the last minute: Kursk's
		// tens of thousands; put in firing order, as the walk over them was, so the heats sum as before)
		{
			IndexLandings();
			auto Lo = std::lower_bound(ByLanding.begin(), ByLanding.end(), From, [this](int K, double V) { return Rec->Rounds[static_cast<size_t>(K)].L < V; });
			auto Hi = std::upper_bound(Lo, ByLanding.end(), To, [this](double V, int K) { return V < Rec->Rounds[static_cast<size_t>(K)].L; });
			GatherIx.clear();
			for (auto It = Lo; It != Hi; ++It)
			{
				const size_t I = static_cast<size_t>(*It);
				if (I >= RoundFrom && Rec->Rounds[I].T <= To) GatherIx.push_back(static_cast<int>(I));
			}
			std::sort(GatherIx.begin(), GatherIx.end());
			for (const int I : GatherIx)
			{
				const LandFeed::Round& R = Rec->Rounds[static_cast<size_t>(I)];
				Ahead.push_back({ R.X, R.Y, R.L, WeightOf(R.W) * (R.R == LandFeed::Result::Kill ? 3.0 : 1.0) });
			}
		}
		for (const Track& Tr : Rec->Tracks)
		{
			if (Tr.Fell < From || Tr.Fell > To || Tr.bTwin) continue;
			const LandFeed::Spot S = LandFeed::SpotAt(Tr, Tr.Fell);
			Ahead.push_back({ S.X, S.Y, Tr.Fell, Tr.IsVehicle() ? 25.0 : 10.0 });
		}
		// (by squares, each heat in its own once, in Ahead's order)
		if (HeatHead.empty()) HeatHead.assign(HeatBuckets, -1);
		for (const uint32_t B : HeatUsed) HeatHead[B] = -1;
		HeatUsed.clear();
		HeatNext.assign(Ahead.size(), -1);
		for (size_t K = Ahead.size(); K-- > 0;)
		{
			const size_t B = BucketOf(static_cast<int64_t>(std::floor(Ahead[K].X / HeatSquare)), static_cast<int64_t>(std::floor(Ahead[K].Y / HeatSquare)), HeatBuckets);
			if (HeatHead[B] < 0) HeatUsed.push_back(static_cast<uint32_t>(B));
			HeatNext[K] = HeatHead[B];
			HeatHead[B] = static_cast<int32_t>(K);
		}
	}

	double Director::HeatNear(double X, double Y, double From, double To, double Radius, double NearFrom, double NearTo, double NearRadius, double& OutNear, long& OutSeen) const
	{
		// (the squares round the point, each heat in them once, marked by its place in Ahead and summed in that order, as
		// a walk over all of it would; the near heat in the same walk, its window and ring inside the first)
		OutNear = 0.0;
		OutSeen = 0;
		if (HeatHead.empty() || Ahead.empty()) return 0.0;
		const size_t Words = (Ahead.size() + 63) / 64;
		if (HeatBits.size() < Words) HeatBits.assign(Words, 0);
		size_t Lo = Words, Hi = 0;
		const int64_t I0 = static_cast<int64_t>(std::floor((X - Radius) / HeatSquare)), I1 = static_cast<int64_t>(std::floor((X + Radius) / HeatSquare));
		const int64_t J0 = static_cast<int64_t>(std::floor((Y - Radius) / HeatSquare)), J1 = static_cast<int64_t>(std::floor((Y + Radius) / HeatSquare));
		const double Reach = (Radius + 0.01) * (Radius + 0.01);
		for (int64_t I = I0; I <= I1; ++I)
		{
			const double Nx = std::clamp(X, static_cast<double>(I) * HeatSquare, static_cast<double>(I + 1) * HeatSquare) - X;
			for (int64_t J = J0; J <= J1; ++J)
			{
				// (a corner square wholly beyond the ring holds none of it)
				const double Ny = std::clamp(Y, static_cast<double>(J) * HeatSquare, static_cast<double>(J + 1) * HeatSquare) - Y;
				if (Nx * Nx + Ny * Ny > Reach) continue;
				for (int32_t K = HeatHead[BucketOf(I, J, HeatBuckets)]; K >= 0; K = HeatNext[static_cast<size_t>(K)])
				{
					++OutSeen;
					const Heat& H = Ahead[static_cast<size_t>(K)];
					// (a heat of another square that shares the chain is left to its own square)
					if (static_cast<int64_t>(std::floor(H.X / HeatSquare)) != I || static_cast<int64_t>(std::floor(H.Y / HeatSquare)) != J) continue;
					if (H.T < From || H.T > To || std::abs(H.X - X) > Radius || std::abs(H.Y - Y) > Radius) continue;
					const size_t W = static_cast<size_t>(K) >> 6;
					HeatBits[W] |= uint64_t(1) << (static_cast<size_t>(K) & 63);
					Lo = std::min(Lo, W);
					Hi = std::max(Hi, W);
				}
			}
		}
		double Sum = 0.0;
		for (size_t W = Lo; W <= Hi && W < Words; ++W)
		{
			uint64_t Bits = HeatBits[W];
			HeatBits[W] = 0;
			while (Bits)
			{
				const size_t K = W * 64 + static_cast<size_t>(std::countr_zero(Bits));
				Bits &= Bits - 1;
				const Heat& H = Ahead[K];
				const double D = std::hypot(H.X - X, H.Y - Y);
				if (D > Radius) continue;
				Sum += H.W * (1.0 - D / Radius);
				if (H.T < NearFrom || H.T > NearTo || std::abs(H.X - X) > NearRadius || std::abs(H.Y - Y) > NearRadius || D > NearRadius) continue;
				OutNear += H.W * (1.0 - D / NearRadius);
			}
		}
		return Sum;
	}

	double Director::BlockedShare(const Vec& Eye, const Vec& Look, double Fov, int Except, double Enough) const
	{
		const Vec Way = Look - Eye;
		if (Way.Length() < 1e-6) return 0.0;
		const Vec Fwd = Way.Unit();
		Vec Right = Vec(Fwd.Y, -Fwd.X, 0.0);
		Right = Right.Length() < 1e-6 ? Vec(1.0, 0.0, 0.0) : Right.Unit();
		const Vec Upward(Right.Y * Fwd.Z - Right.Z * Fwd.Y, Right.Z * Fwd.X - Right.X * Fwd.Z, Right.X * Fwd.Y - Right.Y * Fwd.X);
		const double TanV = std::tan(Fov * 0.5 * Pi / 180.0);
		const double TanH = TanV * 16.0 / 9.0;
		// Seven across and four up, each ray walked out from the lens in steps: something within a few metres of the
		// lens in the way of a ray is a piece of the picture filled near the lens (dark and soft in the game's light).
		constexpr int Across = 7, Down = 4, Rays = Across * Down;
		const int Stop = static_cast<int>(std::floor(Enough * Rays));
		int Hits = 0;
		for (int R = 0; R < Down; ++R)
		{
			for (int C = 0; C < Across; ++C)
			{
				const double Sx = (C + 0.5) / Across * 2.0 - 1.0, Sy = (R + 0.5) / Down * 2.0 - 1.0;
				const Vec Dir = (Fwd + Right * (Sx * TanH) + Upward * (Sy * TanV)).Unit();
				bool bHit = false;
				for (double D = 0.25; D <= Set.NearBlock + 1e-9; D += 0.35)
				{
					const Vec P = Eye + Dir * D;
					if (P.Z < HeightAt(P.X, P.Y))
					{
						// (the ground: the lens nearly in it is a picture filled; farther, the ground seen, as it should be)
						bHit = D <= Set.GroundBlock;
						break;
					}
					if (Here.Solid)
					{
						const int What = Here.Solid(P, Except);
						if (What == 2 || (What == 1 && D <= Set.ManBlock))
						{
							bHit = true;
							break;
						}
					}
				}
				if (bHit && ++Hits > Stop) return static_cast<double>(Hits) / Rays;
			}
		}
		return static_cast<double>(Hits) / Rays;
	}

	bool Director::RoomAt(const Vec& Eye, int Except) const
	{
		if (!Here.Solid) return true;
		if (Here.Solid(Eye, Except) != 0) return false;
		const double ManNear = Set.ManClear;
		const double Steps[3] = { 0.45, ManNear, Set.ThingClear };
		for (const double R : Steps)
		{
			for (int K = 0; K < 14; ++K)
			{
				const int What = Here.Solid(Eye + RoundWay(K) * R, Except);
				if (What == 2 && R <= Set.ThingClear + 1e-9) return false;
				if (What == 1 && R <= ManNear + 1e-9) return false;
			}
		}
		return true;
	}

	double Director::ScreenShare(const Vec& Eye, const Vec& Look, double Fov, int Except, double Enough) const
	{
		const Vec Way = Look - Eye;
		const double Long = Way.Length();
		if (Long < 1e-6) return 0.0;
		const Vec Fwd = Way * (1.0 / Long);
		Vec Right = Vec(Fwd.Y, -Fwd.X, 0.0);
		Right = Right.Length() < 1e-6 ? Vec(1.0, 0.0, 0.0) : Right.Unit();
		const Vec Upward(Right.Y * Fwd.Z - Right.Z * Fwd.Y, Right.Z * Fwd.X - Right.X * Fwd.Z, Right.X * Fwd.Y - Right.Y * Fwd.X);
		const double TanV = std::tan(Fov * 0.5 * Pi / 180.0);
		const double TanH = TanV * 16.0 / 9.0;
		// (out to what it looks at, short of it by a few metres: the men round a watched man, a tracked tank's own side,
		// are what the picture is of)
		const double Far = std::max(Set.NearBlock, std::min(Set.ScreenFar, std::max(0.0, Long - 3.0)));
		// Eight across and five up, each ray walked out from the lens: fine steps near it, longer beyond.
		constexpr int Across = 8, Down = 5, Rays = Across * Down;
		const int Stop = static_cast<int>(std::floor(Enough * Rays));
		int Hits = 0;
		for (int R = 0; R < Down; ++R)
		{
			const double Sy = (R + 0.5) / Down * 2.0 - 1.0;
			for (int C = 0; C < Across; ++C)
			{
				const double Sx = (C + 0.5) / Across * 2.0 - 1.0;
				const Vec Dir = (Fwd + Right * (Sx * TanH) + Upward * (Sy * TanV)).Unit();
				bool bHit = false;
				for (double D = 0.25; D <= Far + 1e-9; D += D < Set.NearBlock ? 0.35 : 1.0)
				{
					const Vec P = Eye + Dir * D;
					if (P.Z < HeightAt(P.X, P.Y))
					{
						// (the ground: the lens nearly in it, or a bank's face across the upper half of the picture)
						bHit = D <= Set.GroundBlock || (Sy > 0.0 && D <= Set.BankNear);
						break;
					}
					if (Here.Solid)
					{
						const int What = Here.Solid(P, Except);
						if (What == 2 || (What == 1 && D <= Set.ManBlock))
						{
							bHit = true;
							break;
						}
					}
				}
				if (bHit && ++Hits > Stop) return static_cast<double>(Hits) / Rays;
			}
		}
		return static_cast<double>(Hits) / Rays;
	}

	bool Director::SeenFrom(const Vec& Eye, const Vec& At, int Except) const
	{
		if (!Here.Solid) return true;
		const Vec Way = At - Eye;
		const double Long = Way.Length();
		if (Long < 4.0) return true;
		const Vec Dir = Way * (1.0 / Long);
		for (double D = 1.5; D <= std::min(Long - 2.5, 150.0); D += 0.8)
		{
			const Vec P = Eye + Dir * D;
			if (P.Z < HeightAt(P.X, P.Y) - 0.3 || Here.Solid(P, Except) == 2) return false;
		}
		return true;
	}

	bool Director::TrackSeen(const Vec& Eye, int Track, double T, double Over) const
	{
		Vec At;
		double Facing = 0.0;
		if (!WhereIs(Track, T, At, Facing)) return false;
		At.Z = HeightAt(At.X, At.Y) + Over;
		return SeenFrom(Eye, At, Track);
	}

	// ---- The camera director's rules (the second cut's frame check, 21 November) -----------------------------------

	double Director::ShoulderUp(double Battle) const
	{
		if (Subject < 0 || !Rec) return Up;
		const Track& Tr = Rec->Tracks[static_cast<size_t>(Subject)];
		if (Tr.IsVehicle()) return Up;
		// (the operator stands, kneels or lies down with him: held at 1.55 m over a man who had gone prone, the second
		// cut's shoulder shots framed a bank's blank face with only his rifle's tip at the bottom)
		const LandFeed::Change* C = LandFeed::ChangeAt(Tr, Battle);
		const std::string Pose = C ? C->Pose : std::string("idle");
		return IsDown(Pose) ? 0.55 : (Pose == "fire_kneel" || Pose == "wait" ? 1.15 : 1.55);
	}

	double Director::LookOver(double UpNow) const
	{
		if (Shot == Kind::Low) return 0.6;
		if (Shot == Kind::Tank) return 1.2;
		return std::min(1.2, 0.2 + 0.8 * UpNow);
	}

	double Director::CloudOf(const LandFeed::Round& R) const
	{
		if (R.W == "bomb") return Set.CloudBomb;
		if (R.W == "gun") return Set.CloudGun;
		if (R.W == "mortar") return Set.CloudMortar;
		return Set.CloudShell;
	}

	bool Director::InFuelBall(const Vec& At, double From, double To, double* OutWhen) const
	{
		if (!Rec || Burnt.empty() || To < From) return false;
		auto It = std::lower_bound(Burnt.begin(), Burnt.end(), From, [this](int I, double V) { return Rec->Tracks[static_cast<size_t>(I)].Fell < V; });
		for (; It != Burnt.end(); ++It)
		{
			const Track& Tr = Rec->Tracks[static_cast<size_t>(*It)];
			if (Tr.Fell > To) break;
			const LandFeed::Spot S = LandFeed::SpotAt(Tr, Tr.Fell);
			// (the ball's middle a couple of metres over the hull, where it seats and climbs)
			if ((Vec(S.X, S.Y, HeightAt(S.X, S.Y) + 2.0) - At).Length() < Set.FuelVehicle)
			{
				if (OutWhen) *OutWhen = Tr.Fell;
				return true;
			}
		}
		return false;
	}

	bool Director::InCloud(const Vec& At, double T) const
	{
		// (a fuel ball in its first seconds is held off as a burst's cloud is)
		if (InFuelBall(At, T - Soon(Set.FuelFor), T)) return true;
		if (!Rec || Heavy.empty()) return false;
		// (the bursts landed in the cloud's first seconds before T, in landing order)
		auto It = std::lower_bound(Heavy.begin(), Heavy.end(), T - Soon(Set.CloudFor),
			[this](int I, double V) { return Rec->Rounds[static_cast<size_t>(I)].L < V; });
		for (; It != Heavy.end(); ++It)
		{
			const LandFeed::Round& R = Rec->Rounds[static_cast<size_t>(*It)];
			if (R.L > T) break;
			const Vec Burst(R.X, R.Y, HeightAt(R.X, R.Y) + 1.0);
			if ((Burst - At).Length() < CloudOf(R)) return true;
		}
		return false;
	}

	bool Director::LensBurst(const Vec& Eye, const Vec& Look, double Fov, double From, double To, int Own, double* OutWhen) const
	{
		// (a tank or a lorry going up near the lens, its own shot's tank too: cut on the flash, as for a shell)
		if (To > From && InFuelBall(Eye, From, To, OutWhen)) return true;
		if (!Rec || Heavy.empty() || !(To > From)) return false;
		// (the first to land after From: the list is in landing order)
		auto It = std::upper_bound(Heavy.begin(), Heavy.end(), From,
			[this](double T, int I) { return T < Rec->Rounds[static_cast<size_t>(I)].L; });
		Pose P;
		P.Eye = Eye;
		P.Look = Look;
		P.Fov = Fov;
		for (; It != Heavy.end(); ++It)
		{
			const LandFeed::Round& R = Rec->Rounds[static_cast<size_t>(*It)];
			if (R.L > To) break;
			const Vec At(R.X, R.Y, HeightAt(R.X, R.Y) + 1.0);
			const double Gap = (At - Eye).Length();
			const double Cloud = CloudOf(R);
			if (Gap >= std::max(Set.BurstClear * Set.BurstBig, Cloud)) continue;
			// (one striking the shot's own man or tank is its story: his fall, her knock-out; only right at the lens is
			// it cut)
			const bool bOwn = Own >= 0 && R.On >= 0 && TrackOfShooter(R.On, R.Ok, R.Oj) == Own;
			const double Clear = Set.BurstClear * (R.W == "gun" ? Set.BurstBig : 1.0);
			// (and any whose cloud will hang round the lens, its own man's or not: the art director's rule)
			if (Gap < Set.BurstAround || Gap < Cloud || (!bOwn && Gap < Clear && InPicture(P, At, 1.3)))
			{
				if (OutWhen) *OutWhen = R.L;
				return true;
			}
		}
		return false;
	}

	double Director::OwnShare(const Vec& Eye, const Vec& Look, double Fov, int Own) const
	{
		if (!Here.Solid || Own < 0) return 0.0;
		const Vec Way = Look - Eye;
		if (Way.Length() < 1e-6) return 0.0;
		const Vec Fwd = Way.Unit();
		Vec Right = Vec(Fwd.Y, -Fwd.X, 0.0);
		Right = Right.Length() < 1e-6 ? Vec(1.0, 0.0, 0.0) : Right.Unit();
		const Vec Upward(Right.Y * Fwd.Z - Right.Z * Fwd.Y, Right.Z * Fwd.X - Right.X * Fwd.Z, Right.X * Fwd.Y - Right.Y * Fwd.X);
		const double TanV = std::tan(Fov * 0.5 * Pi / 180.0);
		const double TanH = TanV * 16.0 / 9.0;
		// (seven across and four up: a ray that meets him before anything else is a piece of the picture he fills)
		constexpr int Across = 7, Down = 4, Rays = Across * Down;
		int Hits = 0;
		for (int R = 0; R < Down; ++R)
		{
			for (int C = 0; C < Across; ++C)
			{
				const double Sx = (C + 0.5) / Across * 2.0 - 1.0, Sy = (R + 0.5) / Down * 2.0 - 1.0;
				const Vec Dir = (Fwd + Right * (Sx * TanH) + Upward * (Sy * TanV)).Unit();
				for (double D = 0.2; D <= 4.0 + 1e-9; D += 0.25)
				{
					const Vec P = Eye + Dir * D;
					if (P.Z < HeightAt(P.X, P.Y)) break;
					// (anything there at all; then whether it is anything but him)
					const int What = Here.Solid(P, -1);
					if (What == 0) continue;
					if (Here.Solid(P, Own) != 0) break;
					if (What == 1)
					{
						++Hits;
						break;
					}
				}
			}
		}
		return static_cast<double>(Hits) / Rays;
	}

	bool Director::HeadAndChest(int Own, double Battle, Vec& OutHead, Vec& OutChest) const
	{
		if (Own < 0 || !Rec) return false;
		const Track& Tr = Rec->Tracks[static_cast<size_t>(Own)];
		if (Tr.IsVehicle()) return false;
		Vec At;
		double Facing = 0.0;
		if (!WhereIs(Own, Battle, At, Facing) || !BodyOf(Own, Battle, OutChest)) return false;
		const bool bFallen = Tr.Fell >= 0.0 && Tr.Fell <= Battle;
		const LandFeed::Change* C = LandFeed::ChangeAt(Tr, Battle);
		const bool bDown = bFallen || (C && IsDown(C->Pose));
		OutHead = bDown ? OutChest + Before(Facing, 0.45) : OutChest + Vec(0.0, 0.0, 0.45);
		return true;
	}

	double Director::OwnDistance(const Vec& Eye, int Own, double Battle) const
	{
		Vec Head, Chest;
		if (!HeadAndChest(Own, Battle, Head, Chest)) return 1e9;
		return std::min((Eye - Head).Length(), (Eye - Chest).Length());
	}

	Vec Director::ClearOfOwn(Vec Eye, double Battle, double Clear) const
	{
		Vec Head, Chest;
		if (!HeadAndChest(Subject, Battle, Head, Chest)) return Eye;
		if (Clear <= 0.0) Clear = Set.OwnClear;
		// (pushed out level from his head and chest, never up or down, so it does not bob as he turns: the second cut's
		// lens a few centimetres from a man's face as he rose out of a shell hole beside it)
		for (const Vec& P : { Head, Chest })
		{
			const double Dz = Eye.Z - P.Z;
			if (std::abs(Dz) >= Clear) continue;
			const double Need = std::sqrt(Clear * Clear - Dz * Dz);
			Vec Away = Eye - P;
			Away.Z = 0.0;
			const double Gap = Away.Length();
			if (Gap >= Need) continue;
			Away = Gap < 1e-6 ? Before(FacingEased + Pi, 1.0) : Away * (1.0 / Gap);
			Eye.X = P.X + Away.X * Need;
			Eye.Y = P.Y + Away.Y * Need;
		}
		return Eye;
	}

	bool Director::Crossing(const Vec* Eyes, const double* Times, int N, int Own, double Clear) const
	{
		const double Near = Clear >= 0.0 ? Clear : Set.CrossClear;
		// (an asked still lets a man run past nearer than that, to AskCross, if he is within Near for no more than
		// AskPass on the screen, where the lens is then)
		const bool bPass = Clear < 0.0 && AskedNow();
		if (!Rec || N <= 0) return false;
		double Moved = 0.0;
		for (int K = 1; K < N; ++K) Moved = std::max(Moved, Flat(Eyes[K], Eyes[0]));
		const double From = Times[0], To = Times[N - 1];
		// (a man runs some 6 m a second at most, 12 on the rules' clock, twice life's: farther off than that, none can
		// reach the lens in time)
		const double Reach = Near + 12.0 * (To - From) + Moved + 0.5;
		// (the men by squares where they stood a moment ago, kept half a battle second: those that could be within Reach
		// now, by how far the fastest man goes, are asked where they are)
		if (MenTracks != Rec->Tracks.size() || std::abs(From - MenT) > 0.5) PlaceMen(From);
		const double Wide = Reach + MenFastest * std::abs(From - MenT) + 0.01;
		const int64_t I0 = static_cast<int64_t>(std::floor((Eyes[0].X - Wide) / MenSquare)), I1 = static_cast<int64_t>(std::floor((Eyes[0].X + Wide) / MenSquare));
		const int64_t J0 = static_cast<int64_t>(std::floor((Eyes[0].Y - Wide) / MenSquare)), J1 = static_cast<int64_t>(std::floor((Eyes[0].Y + Wide) / MenSquare));
		for (int64_t Ia = I0; Ia <= I1; ++Ia)
		{
			for (int64_t Ja = J0; Ja <= J1; ++Ja)
			{
				for (int32_t E = MenHead[BucketOf(Ia, Ja, MenBuckets)]; E >= 0; E = MenNext[static_cast<size_t>(E)])
				{
					const ManAt& Ma = MenAt[static_cast<size_t>(E)];
					// (a man of another square that shares the chain is left to his own square)
					if (static_cast<int64_t>(std::floor(Ma.X / MenSquare)) != Ia || static_cast<int64_t>(std::floor(Ma.Y / MenSquare)) != Ja) continue;
					if (std::abs(Ma.X - Eyes[0].X) > Wide || std::abs(Ma.Y - Eyes[0].Y) > Wide) continue;
					const size_t I = static_cast<size_t>(Ma.Track);
					if (static_cast<int>(I) == Own) continue;
					const Track& Tr = Rec->Tracks[I];
					if (Tr.bTwin || !Tr.IsMan() || Tr.Start() > To || (Tr.Out >= 0.0 && Tr.Out < From)) continue;
					const LandFeed::Spot S0 = LandFeed::SpotAt(Tr, From);
					if (std::abs(S0.X - Eyes[0].X) > Reach || std::abs(S0.Y - Eyes[0].Y) > Reach) continue;
					for (int K = 0; K < N; ++K)
					{
						const LandFeed::Spot S = K == 0 ? S0 : LandFeed::SpotAt(Tr, Times[K]);
						if (std::hypot(S.X - Eyes[K].X, S.Y - Eyes[K].Y) >= Near) continue;
						// (his body from the ground to the top of him: standing, kneeling, lying or fallen)
						const bool bFallen = Tr.Fell >= 0.0 && Tr.Fell <= Times[K];
						const LandFeed::Change* C = LandFeed::ChangeAt(Tr, Times[K]);
						const std::string Pose = C ? C->Pose : std::string("idle");
						// (a man riding in a lorry's bed is the lorry's, a thing the lens keeps off already)
						if (Pose == "mounted") continue;
						const double Top = bFallen || IsDown(Pose) ? 0.45 : (Pose == "fire_kneel" || Pose == "wait" ? 1.1 : 1.75);
						if (Eyes[K].Z >= HeightAt(S.X, S.Y) + Top + 0.3) continue;
						if (bPass && !bFallen && Passes(Tr, Eyes[K], Times[K], Near)) continue;
						return true;
					}
				}
			}
		}
		return false;
	}

	bool Director::Passes(const LandFeed::Track& Tr, const Vec& Eye, double T, double Near) const
	{
		// (back and on from T in steps of 0.05 s on the screen, the lens held where it is then: how long he is within
		// Near of it, and how near he comes)
		// (a step of at most 0.05 battle seconds whatever the pace: at four times life a runner 0.7 m off the lens could
		// slip between steps of 0.2 s, the pre-check's arithmetic)
		const double Step = std::min(Soon(0.05), 0.05), Most = Soon(Set.AskPass);
		double Nearest = 1e9, Span = 0.0;
		for (int Way = -1; Way <= 1; Way += 2)
		{
			for (double U = Way < 0 ? Step : 0.0; ; U += Step)
			{
				const double At = T + Way * U;
				if (At < Tr.Start() || (Tr.Out >= 0.0 && At > Tr.Out)) break;
				const LandFeed::Spot S = LandFeed::SpotAt(Tr, At);
				const double Gap = std::hypot(S.X - Eye.X, S.Y - Eye.Y);
				if (Gap >= Near) break;
				Nearest = std::min(Nearest, Gap);
				Span += Step;
				if (Span > Most || Nearest < Set.AskCross) return false;
			}
		}
		return true;
	}

	int Director::MenInPicture(const Pose& P, double Battle, double Within, int Enough, int Except) const
	{
		if (!Rec) return 0;
		if (MenTracks != Rec->Tracks.size() || std::abs(Battle - MenT) > 0.5) PlaceMen(Battle);
		const double Wide = Within + MenFastest * std::abs(Battle - MenT) + 0.01;
		const int64_t I0 = static_cast<int64_t>(std::floor((P.Eye.X - Wide) / MenSquare)), I1 = static_cast<int64_t>(std::floor((P.Eye.X + Wide) / MenSquare));
		const int64_t J0 = static_cast<int64_t>(std::floor((P.Eye.Y - Wide) / MenSquare)), J1 = static_cast<int64_t>(std::floor((P.Eye.Y + Wide) / MenSquare));
		int Seen = 0;
		for (int64_t Ia = I0; Ia <= I1; ++Ia)
			for (int64_t Ja = J0; Ja <= J1; ++Ja)
				for (int32_t E = MenHead[BucketOf(Ia, Ja, MenBuckets)]; E >= 0; E = MenNext[static_cast<size_t>(E)])
				{
					const ManAt& Ma = MenAt[static_cast<size_t>(E)];
					if (static_cast<int64_t>(std::floor(Ma.X / MenSquare)) != Ia || static_cast<int64_t>(std::floor(Ma.Y / MenSquare)) != Ja) continue;
					if (Ma.Track == Except) continue;
					const Track& Tr = Rec->Tracks[static_cast<size_t>(Ma.Track)];
					if (Tr.bTwin || !Tr.IsMan() || Tr.Start() > Battle || (Tr.Out >= 0.0 && Tr.Out < Battle) || (Tr.Fell >= 0.0 && Tr.Fell <= Battle)) continue;
					const LandFeed::Spot S = LandFeed::SpotAt(Tr, Battle);
					const LandFeed::Change* C = LandFeed::ChangeAt(Tr, Battle);
					if (C && C->Pose == "mounted") continue;
					const Vec Chest(S.X, S.Y, HeightAt(S.X, S.Y) + (C && IsDown(C->Pose) ? 0.35 : 1.0));
					if (Flat(Chest, P.Eye) > Within || !InPicture(P, Chest, 0.95)) continue;
					// (over the ground's own rise as well: from a low lens a fold of the steppe hides a man 30 m off)
					if ((Here.Open && !Here.Open(P.Eye, Chest)) || !SeenFrom(P.Eye, Chest, Ma.Track)) continue;
					if (++Seen >= Enough) return Seen;
				}
		return Seen;
	}

	bool Director::MenCross(const Pose& P, double From, double To) const
	{
		Vec Eyes[9];
		double Times[9];
		int N = 0;
		for (double T = From; T <= To + 1e-9 && N < 9; T += 0.25)
		{
			Eyes[N] = P.Eye;
			Times[N] = T;
			++N;
		}
		return Crossing(Eyes, Times, N, P.Subject);
	}

	double Director::EdgeShare(const Vec& Eye, const Vec& Look, double Fov, int Except) const
	{
		if (!Here.Solid) return 0.0;
		const Vec Way = Look - Eye;
		if (Way.Length() < 1e-6) return 0.0;
		const Vec Fwd = Way.Unit();
		Vec Right = Vec(Fwd.Y, -Fwd.X, 0.0);
		Right = Right.Length() < 1e-6 ? Vec(1.0, 0.0, 0.0) : Right.Unit();
		const Vec Upward(Right.Y * Fwd.Z - Right.Z * Fwd.Y, Right.Z * Fwd.X - Right.X * Fwd.Z, Right.X * Fwd.Y - Right.Y * Fwd.X);
		const double TanV = std::tan(Fov * 0.5 * Pi / 180.0);
		const double TanH = TanV * 16.0 / 9.0;
		// Five across and three up, and twelve round the picture's border (its corners and the middles of its edges,
		// where the second cut's black leaf cards poked in): a leaf or a trunk this near the lens draws black and hard.
		// (the middle of the picture near the lens is BlockedShare's too: fewer rays there, and it stops once the
		// picture is over its share, so a cut's tries stay inside a frame)
		static const double Border[12][2] = {
			{ -0.95, -0.95 }, { 0.95, -0.95 }, { -0.95, 0.95 }, { 0.95, 0.95 }, { 0.0, 0.95 }, { 0.0, -0.95 },
			{ -0.95, 0.0 }, { 0.95, 0.0 }, { -0.5, 0.95 }, { 0.5, 0.95 }, { -0.95, 0.5 }, { 0.95, 0.5 },
		};
		constexpr int Across = 5, Down = 3, Rays = Across * Down + 12;
		const int Stop = static_cast<int>(std::floor(Set.EdgeMost * Rays));
		int Hits = 0;
		for (int K = 0; K < Rays; ++K)
		{
			double Sx = 0.0, Sy = 0.0;
			if (K < Across * Down)
			{
				Sx = (K % Across + 0.5) / Across * 2.0 - 1.0;
				Sy = (K / Across + 0.5) / Down * 2.0 - 1.0;
			}
			else
			{
				Sx = Border[K - Across * Down][0];
				Sy = Border[K - Across * Down][1];
			}
			const Vec Dir = (Fwd + Right * (Sx * TanH) + Upward * (Sy * TanV)).Unit();
			for (double D = 0.2; D <= Set.EdgeNear + 1e-9; D += 0.25)
			{
				const Vec P = Eye + Dir * D;
				if (P.Z < HeightAt(P.X, P.Y)) break;
				const int What = Here.Solid(P, Except);
				if (What == 2 && ++Hits > Stop) return static_cast<double>(Hits) / Rays;
				if (What != 0) break;
			}
		}
		return static_cast<double>(Hits) / Rays;
	}

	bool Director::OwnLost(double Battle) const
	{
		return OwnLostFrom(Now, Battle);
	}

	bool Director::OwnLostFrom(const Pose& From, double Battle) const
	{
		Vec Head, Chest;
		if (!HeadAndChest(Subject, Battle, Head, Chest)) return false;
		if (!InPicture(From, Chest, 0.95) && !InPicture(From, Head, 0.95)) return true;
		// (down in corn taller than he lies, seen from more than a few metres: the last metres of the line to his chest
		// run among the stalks; the second cut held six seconds on a corner of wheat the two men had lain down in)
		const Vec Way = From.Eye - Chest;
		const double Long = Way.Length();
		if (Long < 3.5) return false;
		const Vec Dir = Way * (1.0 / Long);
		int Stalks = 0;
		for (const double D : { 0.7, 1.4, 2.1, 2.8 })
		{
			const Vec P = Chest + Dir * D;
			const double Crop = CropAt(P.X, P.Y);
			if (Crop > 0.0 && P.Z < HeightAt(P.X, P.Y) + Crop) ++Stalks;
		}
		return Stalks >= 2;
	}

	bool Director::MovingIn(double Battle) const
	{
		if (!Rec) return false;
		if (Subject >= 0 && Shot != Kind::Tank && LandFeed::SpotAt(Rec->Tracks[static_cast<size_t>(Subject)], Battle).V > 0.3) return true;
		for (const Track& Tr : Rec->Tracks)
		{
			if (Tr.bTwin || Tr.Start() > Battle || (Tr.Fell >= 0.0 && Tr.Fell <= Battle) || (Tr.Out >= 0.0 && Tr.Out < Battle)) continue;
			const LandFeed::Spot S = LandFeed::SpotAt(Tr, Battle);
			if (S.V <= 0.5) continue;
			const Vec At(S.X, S.Y, HeightAt(S.X, S.Y) + 1.0);
			if ((At - Now.Eye).Length() < 200.0 && InPicture(Now, At, 0.95)) return true;
		}
		return false;
	}

	bool Director::BadPicture(const Vec& Eye, const Vec& Look, double Fov, double Limit) const
	{
		if (Here.Free && !Here.Free(Eye)) return true;
		if (!RoomAt(Eye, Subject)) return true;
		if (BlockedShare(Eye, Look, Fov, Subject, Set.BlockedMost) > Set.BlockedMost) return true;
		if (EdgeShare(Eye, Look, Fov, Subject) > Set.EdgeMost) return true;
		return ScreenShare(Eye, Look, Fov, Subject, Limit) > Limit;
	}

	bool Director::ActionIn(const Pose& In, double From, double To) const
	{
		if (!(To > From)) return false;
		// (the rounds fired in it, from the firing order, and those landing in it, from the landing order: the same as a
		// walk over every round still in the air)
		const auto ByT = [](double T, const LandFeed::Round& R) { return T < R.T; };
		const size_t Fired0 = static_cast<size_t>(std::upper_bound(Rec->Rounds.begin() + static_cast<std::ptrdiff_t>(std::min(RoundFrom, Rec->Rounds.size())), Rec->Rounds.end(), From, ByT) - Rec->Rounds.begin());
		const size_t Past = static_cast<size_t>(std::upper_bound(Rec->Rounds.begin() + static_cast<std::ptrdiff_t>(Fired0), Rec->Rounds.end(), To, ByT) - Rec->Rounds.begin());
		auto Landed0 = std::upper_bound(ByLanding.begin(), ByLanding.end(), From, [this](double V, int K) { return V < Rec->Rounds[static_cast<size_t>(K)].L; });
		for (auto It = Landed0; It != ByLanding.end(); ++It)
		{
			const size_t I = static_cast<size_t>(*It);
			const LandFeed::Round& R = Rec->Rounds[I];
			if (R.L > To) break;
			if (I < RoundFrom || I >= Past) continue;
			const Vec At(R.X, R.Y, HeightAt(R.X, R.Y) + 0.5);
			// (a round that struck someone lands on him: his own hull or body does not hide it)
			const int Struck = R.On >= 0 ? TrackOfShooter(R.On, R.Ok, R.Oj) : -1;
			if ((At - In.Eye).Length() < 300.0 && InPicture(In, At) && SeenFrom(In.Eye, At, Struck)) return true;
		}
		for (size_t I = Fired0; I < Past; ++I)
		{
			const LandFeed::Round& R = Rec->Rounds[I];
			const int Who = TrackOfShooter(R.By, R.K, R.J);
			Vec At;
			double Facing = 0.0;
			if (Who >= 0 && WhereIs(Who, To, At, Facing))
			{
				At.Z += 1.0;
				if ((At - In.Eye).Length() < 250.0 && InPicture(In, At) && SeenFrom(In.Eye, At, Who)) return true;
			}
		}
		for (const Track& Tr : Rec->Tracks)
		{
			if (Tr.bTwin || !(Tr.Fell > From && Tr.Fell <= To)) continue;
			const LandFeed::Spot S = LandFeed::SpotAt(Tr, Tr.Fell);
			const Vec At(S.X, S.Y, HeightAt(S.X, S.Y) + 0.8);
			if ((At - In.Eye).Length() < 300.0 && InPicture(In, At) && SeenFrom(In.Eye, At, static_cast<int>(&Tr - Rec->Tracks.data()))) return true;
		}
		return false;
	}

	bool Director::Place(Kind K, int TrackIndex, double Battle, int Variant)
	{
		Shot = K;
		Subject = TrackIndex;
		ShotHouse = K == Kind::Watch ? TryHouse : -1;
		bShotSalvo = K == Kind::Watch && bTrySalvo;
		ShotAim = TryAim;
		FellAt = -1.0;
		bFallEye = false;
		bMassNow = false;
		// (the whole mass: its own place, over the own side's rear)
		if (K == Kind::Wide && TrackIndex == MassSubject)
		{
			Subject = -1;
			bMassNow = true;
			return PlaceMass(Battle);
		}
		const double Fw = Rec->Ground.W > 0 ? Rec->Ground.W : 3000.0;
		const double Fh = Rec->Ground.H > 0 ? Rec->Ground.H : 2000.0;
		if (K == Kind::Open || K == Kind::Wide)
		{
			// Behind a side's line, across the field toward the other.
			const int Whose = K == Kind::Open ? 0 : (Roll() < 0.75 ? Set.Own : 1 - Set.Own);
			double Sx = 0.0, Sy = 0.0, Ex = 0.0, Ey = 0.0;
			int Ns = 0, Ne = 0;
			for (size_t I = 0; I < Rec->Tracks.size(); ++I)
			{
				const Track& Tr = Rec->Tracks[I];
				if (Tr.bTwin || (Tr.Fell >= 0.0 && Tr.Fell < Battle)) continue;
				const LandFeed::Block* B = BlockOfTrack(static_cast<int>(I));
				if (!B || (B->Kind == "art" && B->Team.empty())) continue; // the batteries stand far back
				if (HiddenNow(static_cast<int>(I))) continue; // (nor where an ambush lies, as the player does not know it)
				const LandFeed::Spot S = LandFeed::SpotAt(Tr, Battle);
				if (B->Side == Whose) { Sx += S.X; Sy += S.Y; ++Ns; }
				else { Ex += S.X; Ey += S.Y; ++Ne; }
			}
			if (Ns == 0 || Ne == 0) { Sx = Fw * 0.5; Sy = Whose == 0 ? Fh * 0.25 : Fh * 0.75; Ns = 1; Ex = Fw * 0.5; Ey = Whose == 0 ? Fh * 0.75 : Fh * 0.25; Ne = 1; }
			Sx /= Ns; Sy /= Ns; Ex /= Ne; Ey /= Ne;
			const double Toward = LandFeed::FacingTo(Sx, Sy, Ex, Ey);
			const double BackM = K == Kind::Open ? 420.0 : 160.0 + Roll() * 140.0;
			const double SideM = (Roll() * 2.0 - 1.0) * (K == Kind::Open ? 120.0 : 220.0);
			const Vec Base = Vec(Sx, Sy, 0.0) + Offset(Toward, BackM, SideM);
			Stand = Vec(Base.X, Base.Y, HeightAt(Base.X, Base.Y) + (K == Kind::Open ? 170.0 : 35.0 + Roll() * 35.0));
			const Vec Mid = Vec(Sx + (Ex - Sx) * 0.6, Sy + (Ey - Sy) * 0.6, 0.0);
			Target = Vec(Mid.X, Mid.Y, HeightAt(Mid.X, Mid.Y) + 2.0);
			Now.Fov = K == Kind::Open ? 48.0 : 42.0 + Roll() * 10.0;
			return true;
		}
		Vec At;
		double Facing = 0.0;
		if (!WhereIs(TrackIndex, Battle, At, Facing)) return false;
		const Track& Tr = Rec->Tracks[static_cast<size_t>(TrackIndex)];
		const LandFeed::Change* C = LandFeed::ChangeAt(Tr, Battle);
		const std::string Pose = C ? C->Pose : std::string("idle");
		FacingEased = Facing;
		// (each try after the first stands on the other side, and every second one farther off or higher)
		if (Variant == 0) VariantSide = Roll() < 0.5 ? -1.0 : 1.0;
		const double SideSign = Variant % 2 == 0 ? VariantSide : -VariantSide;
		const double Further = static_cast<double>(Variant / 2);
		switch (K)
		{
		case Kind::Shoulder:
			Back = 1.3 + Roll() * 0.9 + 0.5 * Further;
			Side = SideSign * (0.45 + Roll() * 0.4);
			// (its height goes with his stance through the shot: ShoulderUp)
			UpJitter = IsDown(Pose) || Pose == "fire_kneel" || Pose == "wait" ? 0.0 : Roll() * 0.12;
			Up = ShoulderUp(Battle) + UpJitter;
			Now.Fov = 50.0 + Roll() * 12.0;
			break;
		case Kind::Low:
			if (Tr.Fell >= 0.0 && Tr.Fell <= Battle)
			{
				// A man who has fallen: low in the grass a few metres off him, looking at him (the ground's cut 5 still 09 on
				// the scene's bodies; on the recording's alone, beside him the low lens had him out of the picture)
				Back = 2.4 + Roll() * 1.4 + 0.6 * Further;
				Side = SideSign * (0.4 + Roll() * 0.9);
				Up = 0.45 + Roll() * 0.2;
				Now.Fov = 40.0 + Roll() * 10.0;
				break;
			}
			// Beside him in the grass, level with his shoulders, not behind his boots (the second cut's low shot looked up
			// two men's boots and packs for 4 s).
			Back = -0.3 + Roll() * 0.7 + 0.4 * Further;
			Side = SideSign * (1.0 + Roll() * 0.6);
			Up = 0.32 + Roll() * 0.15;
			Now.Fov = 46.0 + Roll() * 10.0;
			break;
		case Kind::Tank:
			// (riding with her: behind the turret and over the hatch, looking down her gun)
			Back = 3.0 + Roll() * 2.0 + 0.8 * Further;
			Side = SideSign * (0.3 + Roll() * 0.6);
			Up = 4.0 + Roll() * 1.0 + 0.4 * Further;
			Now.Fov = 55.0 + Roll() * 8.0;
			break;
		case Kind::Track:
		{
			// From 20 to 40 m off her side, a little ahead of her, a long lens: she drives and fires across the picture.
			const LandFeed::Spot S = LandFeed::SpotAt(Tr, Battle);
			const double Way = S.V > 0.3 ? S.Way : S.F;
			const double Away = std::max(14.0, 20.0 + Roll() * 20.0 - 4.0 * Further);
			TrackOff = Offset(Way, -(2.0 + Roll() * 6.0), SideSign * Away);
			TrackUp = 1.5 + Roll() * 1.5 + 0.8 * Further;
			Stand = At + TrackOff;
			Stand.Z = HeightAt(Stand.X, Stand.Y) + TrackUp;
			Target = At + Vec(0.0, 0.0, 1.4);
			Now.Fov = 16.0 + Roll() * 7.0;
			return !Here.Open || Here.Open(Stand, Target);
		}
		case Kind::Watch:
		{
			// The village still: the house behind him or beside him, the eye off the far side of him from it, 22 to 46 m,
			// a lens wide enough for both (the look turned HouseLook of the way toward the house). The barrage's cut the
			// same, the salvo's middle for the house, 15 m across.
			if (ShotHouse >= 0 || bShotSalvo)
			{
				const double From = LandFeed::FacingTo(ShotAim.X, ShotAim.Y, At.X, At.Y);
				const double Angle = From + SideSign * (0.22 + 0.2 * Further + Roll() * 0.18);
				const double Away = 22.0 + Roll() * 14.0 + 10.0 * Further;
				const Vec Base = At + Vec(std::sin(Angle) * Away, std::cos(Angle) * Away, 0.0);
				Stand = Vec(Base.X, Base.Y, HeightAt(Base.X, Base.Y) + 2.0 + Roll() * 2.5 + Further);
				Target = At + Vec(0.0, 0.0, 1.0);
				// (half the picture across holds the man's and the house's angle apart, less the look's turn, the house's
				// own half-width and a margin)
				const double ManX = At.X - Stand.X, ManY = At.Y - Stand.Y, HouseX = ShotAim.X - Stand.X, HouseY = ShotAim.Y - Stand.Y;
				const double ManLong = std::hypot(ManX, ManY), HouseLong = std::hypot(HouseX, HouseY);
				const double Between = ManLong > 0.0 && HouseLong > 0.0 ? std::acos(std::clamp((ManX * HouseX + ManY * HouseY) / (ManLong * HouseLong), -1.0, 1.0)) : 0.0;
				const double AimR = ShotHouse >= 0 ? std::min(Houses[static_cast<size_t>(ShotHouse)].R, 8.0) : 15.0;
				const double HalfAcross = (1.0 - AimLook()) * Between + std::atan2(AimR, std::max(1.0, HouseLong)) + 0.06;
				const double Needed = 2.0 * std::atan(std::tan(std::min(HalfAcross, 1.4)) * 9.0 / 16.0) * 180.0 / Pi;
				// (the barrage from our line hundreds of metres off: a longer lens, no wider than it needs)
				Now.Fov = bShotSalvo ? std::clamp(std::max(14.0 + Roll() * 6.0, Needed), 12.0, 52.0) : std::clamp(std::max(24.0 + Roll() * 8.0, Needed), 18.0, 52.0);
				return !Here.Open || Here.Open(Stand, Target);
			}
			// Near, for a fall about to come: off his side at a man's height, a long lens. Otherwise from a bank or a rise
			// off his own side's rear quarter, a long lens on him and the men round him.
			const double Away = bNearWatch ? 10.0 + Roll() * 8.0 + 3.0 * Further : 28.0 + Roll() * 27.0 + 8.0 * Further;
			const double Angle = bNearWatch ? Facing + SideSign * (0.5 * Pi + (Roll() - 0.5) * 0.8) : Facing + Pi + SideSign * (0.5 + Roll() * 0.6);
			const Vec Base = At + Vec(std::sin(Angle) * Away, std::cos(Angle) * Away, 0.0);
			const double High = bNearWatch ? 1.6 + Roll() * 1.2 : 3.0 + Roll() * 4.0 + 1.5 * Further;
			Stand = Vec(Base.X, Base.Y, HeightAt(Base.X, Base.Y) + High);
			Target = At + Vec(0.0, 0.0, Tr.IsVehicle() ? 1.3 : 1.0);
			Now.Fov = bNearWatch ? 28.0 + Roll() * 8.0 : 16.0 + Roll() * 8.0;
			return !Here.Open || Here.Open(Stand, Target);
		}
		default:
			break;
		}
		return true;
	}

	void Director::Intended(double Battle, Vec& OutEye, Vec& OutLook) const
	{
		OutEye = Stand;
		OutLook = Target;
		if (Shot == Kind::Open || Shot == Kind::Wide || Subject < 0) return;
		Vec At;
		double Facing = 0.0;
		if (!WhereIs(Subject, Battle, At, Facing)) return;
		if (Shot == Kind::Watch)
		{
			Vec B;
			OutLook = BodyOf(Subject, Battle, B) ? B : At + Vec(0.0, 0.0, 1.0);
			if (ShotHouse >= 0 || bShotSalvo) OutLook = OutLook + (ShotAim - OutLook) * AimLook();
		}
		else if (Shot == Kind::Track)
		{
			OutEye = At + TrackOff;
			OutEye.Z = HeightAt(OutEye.X, OutEye.Y) + TrackUp;
			OutLook = At + Vec(0.0, 0.0, 1.3);
		}
		else
		{
			const double UpNow = Shot == Kind::Shoulder ? ShoulderUp(Battle) + UpJitter : Up;
			OutEye = At + Offset(FacingEased, Back, Side) + Vec(0.0, 0.0, UpNow);
			OutLook = At + Before(FacingEased, Shot == Kind::Tank ? 70.0 : 35.0);
			OutLook.Z = HeightAt(OutLook.X, OutLook.Y) + LookOver(UpNow);
			// (the low shot of a man who has fallen looks at him)
			const Track& St = Rec->Tracks[static_cast<size_t>(Subject)];
			Vec Lying;
			if (Shot == Kind::Low && St.Fell >= 0.0 && St.Fell <= Battle && BodyOf(Subject, Battle, Lying)) OutLook = Lying + Vec(0.0, 0.0, 0.1);
			if (Shot != Kind::Tank) OutEye = ClearOfOwn(OutEye, Battle);
			const double Crop = CropAt(OutEye.X, OutEye.Y);
			if (Crop > 0.0) OutEye.Z = std::max(OutEye.Z, HeightAt(OutEye.X, OutEye.Y) + Crop + OverEars);
		}
		OutEye = KeepAbove(OutEye, Shot == Kind::Low ? 0.25 : 0.9);
	}

	int Director::FitsParts() const
	{
		if (Shot == Kind::Open || Shot == Kind::Wide) return 0;
		return !bLoose && (Shot == Kind::Track || Shot == Kind::Tank || Shot == Kind::Shoulder || Shot == Kind::Low) ? 4 : 2;
	}

	bool Director::FitsPart(double Battle, int Part) const
	{
		// (the cheap asks first, the rays last: a cut tries up to sixteen places, a frame's share at a time)
		Vec E, L;
		Intended(Battle, E, L);
		// (a tank's own shot, riding her into the woods, never looser: what is ahead of her is what it is of, and in
		// Sedan's woods the loose ones were a third trees from start to end; nor one over a man's shoulder or beside him,
		// trunks a third of it for three seconds: the long lenses alone may look through the edge of a wood)
		const bool bScreenLoose = bLoose && (Shot == Kind::Watch || Shot == Kind::Track);
		if (Part == 1)
		{
			// (the village still: its house in the picture and seen)
			if (ShotHouse >= 0 && !HouseSeenIn(E, L, Now.Fov, ShotHouse)) return false;
			// (the barrage's cut: the salvo's middle in the picture)
			if (bShotSalvo)
			{
				Pose Seen;
				Seen.Eye = E;
				Seen.Look = L;
				Seen.Fov = Now.Fov;
				if (!InPicture(Seen, ShotAim, 0.85)) return false;
				// (and still in it as it lands, the man having walked on and the look with him)
				if (SalvoLand > Battle)
				{
					Intended(SalvoLand, Seen.Eye, Seen.Look);
					if (!InPicture(Seen, ShotAim, 0.8)) return false;
				}
				// (and not a speck: the salvo's 15 m a twentieth of the picture's height at the least)
				if (15.0 / (2.0 * std::max(1.0, (ShotAim - E).Length()) * std::tan(0.5 * Now.Fov * Pi / 180.0)) < 0.05) return false;
			}
			return !BadPicture(E, L, Now.Fov, bScreenLoose ? Set.ScreenLeave : Set.ScreenMost);
		}
		// (and a shot that follows its subject still a fit picture where the eye will be in a moment: not about to drive
		// into a hedge)
		if (Part >= 2)
		{
			// (a tank's as strictly: placed clear, she drove into the trees from the second second on, screened 30 to 45%
			// for four seconds)
			Intended(Battle + Soon(Part == 2 ? 1.5 : 3.0), E, L);
			return !BadPicture(E, L, Now.Fov, Shot == Kind::Tank ? Set.ScreenMost : Set.ScreenLeave);
		}
		// (nowhere near where a shot was left for a dark picture: the same hedge's shaded face again)
		for (const Vec& Was : DarkSpots)
			if (Flat(Was, E) < Set.DarkClear) return false;
		// (a place on the ground among the men is used once in the film: the third cut ran the same over-the-shoulder
		// by a dark bush three times)
		// (an asked still may: the picture run asks the same stills of every light it tries; but not where the same ask
		// was just left)
		if ((Shot == Kind::Shoulder || Shot == Kind::Low) && !bLoose && !bChoiceAsked)
			for (const Vec& Was : UsedSpots)
				if (Flat(Was, E) < Set.SpotOnce) return false;
		if (bChoiceAsked)
			for (const Vec& Was : AskLeft)
				if (Flat(Was, E) < Set.SpotOnce) return false;
		// (no man of the recording walking into the lens in the next two seconds, where the eye will be then)
		{
			Vec Eyes[5];
			double Times[5];
			for (int K = 0; K < 5; ++K)
			{
				Times[K] = Battle + Soon(0.5 * K);
				Vec Lk;
				if (K == 0) Eyes[K] = E;
				else Intended(Times[K], Eyes[K], Lk);
			}
			if (Crossing(Eyes, Times, 5, Subject)) return false;
		}
		// (no shell landing at the lens in its first seconds: the second cut went near black for 0.8 s; nor its lens in a
		// burst's cloud still hanging, the art director's 4 s by the round's size)
		for (double U = 0.0; U < Set.ShotLeast + 0.5; U += 1.0)
		{
			const double T = Battle + Soon(U);
			Vec Ek = E, Lk = L;
			if (U > 0.0) Intended(T, Ek, Lk);
			if (LensBurst(Ek, Lk, Now.Fov, T - Soon(0.5), T + Soon(0.5), Subject, nullptr) || InCloud(Ek, T)) return false;
		}
		// (a watched man or a tracked tank in sight, not behind a tree or a hedge's shrubs)
		if ((Shot == Kind::Watch || Shot == Kind::Track) && !SeenFrom(E, L, Subject)) return false;
		// (its own man in the picture from its first frame, not down in the corn out of sight: a watch shot opened on a
		// man already lost to it, and held five seconds)
		const Track* Own = Subject >= 0 ? &Rec->Tracks[static_cast<size_t>(Subject)] : nullptr;
		const bool bOwnUp = Own && !(Own->Fell >= 0.0 && Own->Fell <= Battle);
		if (!bLoose && Shot != Kind::Tank && Shot != Kind::Track && Own && Own->IsMan() && !(bChoiceAsked && Shot == Kind::Low && bOwnUp))
		{
			Pose At;
			At.Eye = E;
			At.Look = L;
			At.Fov = Now.Fov;
			if (OwnLostFrom(At, Battle)) return false;
		}
		// (an asked low shot on a man up keeps him beside the lens, so its picture is the men ahead: some of them in it;
		// Kursk's 04 was a field of corn with no one in it)
		if (bChoiceAsked && Shot == Kind::Low && bOwnUp && Own && Own->IsMan())
		{
			Pose At;
			At.Eye = E;
			At.Look = L;
			At.Fov = Now.Fov;
			if (MenInPicture(At, Battle, 60.0, 1, Subject) < 1) return false;
		}
		// (its own man not filling the picture: the second cut's 1.4 s of a man's back and seat against a bank)
		if ((Shot == Kind::Shoulder || Shot == Kind::Low) && OwnShare(E, L, Now.Fov, Subject) > Set.OwnMost) return false;
		return true;
	}

	Director::ShotKept Director::KeepShot() const
	{
		ShotKept K;
		K.Shot = Shot;
		K.Subject = Subject;
		K.FellAt = FellAt;
		K.Fov = Now.Fov;
		K.FacingEased = FacingEased;
		K.VariantSide = VariantSide;
		K.Back = Back;
		K.Side = Side;
		K.Up = Up;
		K.UpJitter = UpJitter;
		K.TrackUp = TrackUp;
		K.bFallEye = bFallEye;
		K.bNearWatch = bNearWatch;
		K.bLoose = bLoose;
		K.bMass = bMassNow;
		K.Stand = Stand;
		K.Target = Target;
		K.TrackOff = TrackOff;
		K.House = ShotHouse;
		K.Aim = ShotAim;
		K.bSalvo = bShotSalvo;
		return K;
	}

	void Director::PutShot(const ShotKept& K)
	{
		Shot = K.Shot;
		Subject = K.Subject;
		FellAt = K.FellAt;
		Now.Fov = K.Fov;
		FacingEased = K.FacingEased;
		VariantSide = K.VariantSide;
		Back = K.Back;
		Side = K.Side;
		Up = K.Up;
		UpJitter = K.UpJitter;
		TrackUp = K.TrackUp;
		bFallEye = K.bFallEye;
		bNearWatch = K.bNearWatch;
		bLoose = K.bLoose;
		bMassNow = K.bMass;
		Stand = K.Stand;
		Target = K.Target;
		ShotHouse = K.House;
		ShotAim = K.Aim;
		bShotSalvo = K.bSalvo;
		TrackOff = K.TrackOff;
	}

	void Director::BeginChoice(double Battle, double Real)
	{
		const double Contact = Rec->Contact >= 0.0 ? Rec->Contact : 0.0;
		const bool bFighting = Battle >= Contact - 5.0;
		Options.clear();
		bScanning = false;
		bool bAskedNow = false;
		// (riding with a unit none of whose men still stands, or hidden from the player now: let go)
		if (RideBlock >= 0 && !RideStands(Battle)) RideBlock = -1;
		// (the general's last order, if it is still fresh and the film is not riding with a unit: weighed below, after
		// the big moments)
		const bool bLeanLive = LeanBlock >= 0 && RideBlock < 0 && (LeanUntil < 0.0 || Real <= LeanUntil);
		if (LeanBlock >= 0 && !bLeanLive)
		{
			LeanBlock = -1;
			LeanUntil = -1.0;
		}
		int Coming = Pending;
		Pending = -1;
		if (Coming >= 0 && OffRide(Coming)) Coming = -1;
		// (a tank about to be knocked out comes before a man's fall the last cut was going to; and one due before this
		// shot could be left for her, gone to now: the camera cut to a man 3.4 s before a tank's end, held its least
		// three seconds, and her window was gone by a frame)
		if ((Coming < 0 || !Rec->Tracks[static_cast<size_t>(Coming)].IsVehicle()) && !bAsked)
		{
			for (size_t I = 0; I < Rec->Tracks.size(); ++I)
			{
				const Track& Tr = Rec->Tracks[I];
				if (Tr.Fell < Battle + Soon(1.0) || Tr.Fell > Battle + Soon(Set.ShotLeast + 2.2) || Tr.bTwin || !IsVehicleAt(I)) continue;
				if (HiddenNow(static_cast<int>(I)) || OffRide(static_cast<int>(I))) continue;
				if (std::find(Appointed.begin(), Appointed.end(), static_cast<int>(I)) != Appointed.end()) continue;
				Appointed.push_back(static_cast<int>(I));
				Coming = static_cast<int>(I);
				break;
			}
		}
		TryHouse = -1;
		AskTries.clear();
		// (radio call, then the barrage: the cut to the caller asked, or his guns' salvo near enough to be gone to now; a
		// tank's end or a fall coming, an asked still and the closing aerial first)
		bRadioChoice = false;
		bTrySalvo = false;
		int RadioWho[4] = { -1, -1, -1, -1 }, RadioMany = 0;
		if (!bAsked && Coming < 0 && RadioStage > 0 && RadioNow >= 0 && !(Battle > Rec->End + 1.0 && !bClosed))
		{
			const Radio& Rd = Radios[static_cast<size_t>(RadioNow)];
			if (RadioStage == 1)
			{
				if (!HiddenNow(Rd.Caller) && !OffRide(Rd.Caller)) RadioWho[RadioMany++] = Rd.Caller;
			}
			else if (Battle >= Rd.Land - Soon(Set.RadioLead + Set.ShotLeast + 0.5)) RadioMany = RadioMen(Rd, Battle, RadioWho);
		}
		if (bAsked)
		{
			bAsked = false;
			bAskedNow = true;
			bAskedUnfit = false;
			// (each asked still from its own throw of the dice, not the film's run so far: the photographer's light A/Bs
			// ask the same stills of runs whose cuts before them part. Thrown by what is asked and how many times it was
			// asked again, not by a count of asks: a still asked again in one run only once it was taken moved the dice
			// of every still after it, Kursk's last four)
			const uint64_t Of = 1 + static_cast<uint64_t>(AskedKind) * 7919ull + static_cast<uint64_t>(AskedSubject + 2) * 104729ull
				+ (bAskedBuilding ? 31ull : 0ull) + static_cast<uint64_t>(AskAgain) * 613ull;
			Dice = (Set.Seed + 0x51ED27ull * Of) * 0xD1B54A32D192ED03ull + 11;
			MakeAskTries(Battle);
			Options.push_back({ AskedKind, AskTries.empty() ? AskedSubject : AskTries.front().Track, false });
			Coming = -1;
		}
		else if (Battle > Rec->End + 1.0 && !bClosed)
		{
			// The closing aerial, once.
			Options.push_back({ Kind::Wide, -1, false });
			Coming = -1;
		}
		else if (RadioMany > 0)
		{
			// Radio call, then the barrage: the man calling, near, as he begins; then the men under the fire, from a little
			// off so the salvo lands in the picture round them.
			bRadioChoice = true;
			const bool bCaller = RadioStage == 1;
			if (!bCaller)
			{
				const Radio& Rd = Radios[static_cast<size_t>(RadioNow)];
				bTrySalvo = true;
				TryAim = Vec(Rd.X, Rd.Y, HeightAt(Rd.X, Rd.Y) + 1.0);
				SalvoLand = Rd.Land;
			}
			for (int K = 0; K < RadioMany; ++K) Options.push_back({ Kind::Watch, RadioWho[K], bCaller });
			if (bCaller) Options.push_back({ Kind::Watch, RadioWho[0], false });
			// (none of them fit: the field weighed away from them, as for any cut that found no place, not the aerial)
			for (int K = 0; K < 4; ++K) ChoiceTop[K] = K < RadioMany ? Pick{ 0.0, RadioWho[K] } : Pick();
			bScanOwnTurn = true;
			bScanTanksOnly = bScanSquadOnly = false;
			bScanRide = RideBlock >= 0;
			ScanBlock = RideBlock;
			bScanFallsDue = Real - LastFallCut >= 6.0;
			bScanFighting = bFighting;
			Coming = -1;
		}
		else if (MassDue(Battle, Coming))
		{
			// The whole mass, once in the middle of the battle (Settings::MassFrom).
			Options.push_back({ Kind::Wide, MassSubject, false });
			Coming = -1;
		}
		else
		{
			// The tanks' turn: no tank or tracking shot in the last two, so this one goes to a tank of either side (a
			// battle of hundreds of men would otherwise never leave them: Brian's moving tanks, a third of the shots).
			bool bTankTurn = bFighting && Recent.size() >= 2 && RideBlock < 0 && !bLeanLive;
			for (size_t R = Recent.size() >= 2 ? Recent.size() - 2 : 0; R < Recent.size(); ++R)
				if (Recent[R] == Kind::Tank || Recent[R] == Kind::Track) bTankTurn = false;
			// The big moments a second or two ahead: a tank knocked out, a man falling. The camera is there just before,
			// as a film's is (the recording is the script): every tank's, some men's (none in the tanks' turn), never
			// one on another.
			if (Coming < 0 && !bHeld)
			{
				for (size_t I = 0; I < Rec->Tracks.size(); ++I)
				{
					const Track& Tr = Rec->Tracks[I];
					if (Tr.bTwin || Tr.Fell < Battle + Soon(1.0) || Tr.Fell > Battle + Soon(3.0) || HiddenNow(static_cast<int>(I)) || OffRide(static_cast<int>(I))) continue;
					if (std::find(Appointed.begin(), Appointed.end(), static_cast<int>(I)) != Appointed.end()) continue;
					Appointed.push_back(static_cast<int>(I));
					if (!Tr.IsVehicle() && (bTankTurn || Real - LastFallCut < 6.0 || Roll() > 0.75)) continue;
					Coming = static_cast<int>(I);
					break;
				}
			}
			if (Coming >= 0)
			{
				const Track& Tr = Rec->Tracks[static_cast<size_t>(Coming)];
				if (Tr.IsVehicle())
				{
					Options.push_back({ Kind::Track, Coming, false });
					Options.push_back({ Kind::Watch, Coming, false });
					Options.push_back({ Kind::Watch, Coming, true });
				}
				else
				{
					// Seen from his side, near, at a man's height; else over the shoulder of a man beside him.
					Options.push_back({ Kind::Watch, Coming, true });
					const LandFeed::Spot Sf = LandFeed::SpotAt(Tr, Battle);
					const LandFeed::Block* Bf = BlockOfTrack(Coming);
					int Mate = -1;
					double MateGap = 1e9;
					for (size_t I = 0; I < Rec->Tracks.size(); ++I)
					{
						const Track& Tm = Rec->Tracks[I];
						if (static_cast<int>(I) == Coming || Tm.bTwin || IsVehicleAt(I) || Tm.Start() > Battle) continue;
						if (Tm.Fell >= 0.0 && Tm.Fell < Battle + Soon(6.0)) continue;
						const LandFeed::Block* Bm = BlockOfTrack(static_cast<int>(I));
						if (!Bm || !Bf || Bm->Side != Bf->Side) continue;
						if (HiddenNow(static_cast<int>(I)) || OffRide(static_cast<int>(I))) continue;
						const LandFeed::Spot Sm = LandFeed::SpotAt(Tm, Battle);
						const double Gap = std::hypot(Sm.X - Sf.X, Sm.Y - Sf.Y);
						if (Gap < 3.0 || Gap > 14.0 || Gap >= MateGap) continue;
						MateGap = Gap;
						Mate = static_cast<int>(I);
					}
					if (Mate >= 0)
					{
						const LandFeed::Change* Cm = LandFeed::ChangeAt(Rec->Tracks[static_cast<size_t>(Mate)], Battle);
						Options.push_back({ Cm && IsDown(Cm->Pose) ? Kind::Low : Kind::Shoulder, Mate, false });
					}
					Options.push_back({ Kind::Watch, Coming, false });
				}
			}
			// The general's order, with no big moment coming: this cut is to the men he sent (weighed as when riding).
			const int LeanNow = Coming < 0 && bLeanLive ? LeanBlock : -1;
			if (LeanNow >= 0)
			{
				LeanBlock = -1;
				LeanUntil = -1.0;
			}
			// The rest: the men and tanks still up, by what is about to happen round them, most of all in the next
			// two seconds (cut on action: to what fires, bursts or falls next).
			Gather(Battle + Soon(0.5), Battle + Soon(8.0));
			bool bOwnTurn = Roll() < Set.OwnShare;
			// (the enemy's turn with every enemy hidden from the player, an ambush not found: his own side's instead, not
			// an aerial for want of a subject)
			if (!bOwnTurn && Here.Hidden)
			{
				bool bEnemySeen = false;
				for (size_t I = 0; I < Rec->Tracks.size() && !bEnemySeen; ++I)
				{
					const Track& Tr = Rec->Tracks[I];
					const LandFeed::Block* B = BlockOfTrack(static_cast<int>(I));
					bEnemySeen = B && B->Side != Set.Own && !Tr.bTwin && Tr.Start() <= Battle && !(Tr.Fell >= 0.0 && Tr.Fell < Battle) && !HiddenNow(static_cast<int>(I));
				}
				bOwnTurn = !bEnemySeen;
			}
			// A man about to fall draws the camera only as often as a fall is cut to (a crowd has a fall every second or
			// two; the film shows some, not all, and not one after another).
			const bool bFallsDue = Real - LastFallCut >= 6.0;
			const double SoonFall = Battle + Soon(0.5), SoonOut = Battle + Soon(4.0);
			auto Standing = [Battle, SoonFall, SoonOut](const Track& Tr)
			{
				return !Tr.bTwin && !(Tr.Fell >= 0.0 && Tr.Fell < SoonFall) && Tr.Start() <= Battle && !(Tr.Out >= 0.0 && Tr.Out < SoonOut);
			};
			// (in the tanks' turn only tanks are weighed, of both sides, while one still stands)
			bool bTanksOnly = false;
			if (bTankTurn)
				for (size_t I = 0; I < Rec->Tracks.size() && !bTanksOnly; ++I) bTanksOnly = KindOf(I) == KindTank && Standing(Rec->Tracks[I]);
			// (and in the film's own section's turn, every SquadEvery shots, only its men, while one still stands)
			bool bSquadOnly = false;
			if (!bTanksOnly && RideBlock < 0 && LeanNow < 0 && HeroBlock >= 0 && SinceSquad >= Set.SquadEvery - 1)
				for (size_t I = 0; I < Rec->Tracks.size() && !bSquadOnly; ++I)
				{
					// (not while they ride in their lorries: Sedan's section sat in the lorry beds for its first 70 s)
					if (!InSquad(static_cast<int>(I)) || !Standing(Rec->Tracks[I])) continue;
					const LandFeed::Change* Ci = LandFeed::ChangeAt(Rec->Tracks[I], Battle);
					bSquadOnly = !(Ci && Ci->Pose == "mounted");
				}
			// (the weighing itself goes on in ScanChoice, a frame's share at a time)
			bScanning = true;
			ChoiceScan = 0;
			for (Pick& P : ChoiceTop) P = Pick();
			bScanOwnTurn = bOwnTurn;
			bScanTanksOnly = bTanksOnly;
			bScanSquadOnly = bSquadOnly;
			bScanRide = RideBlock >= 0 || LeanNow >= 0;
			ScanBlock = RideBlock >= 0 ? RideBlock : LeanNow;
			bScanFallsDue = bFallsDue;
			bScanFighting = bFighting;
		}
		// (the turns asked of the men: about a thirty-second of a ray's step each, in the choice's count)
		RaySteps += static_cast<long>(Rec->Tracks.size() / 32);
		bChoiceAsked = bAskedNow;
		bChoiceMiddle = !bAskedNow && Battle > 12.0 && Battle < Rec->End;
		ScanElsewhere = 0;
		ScanAvoid.clear();
		bScanMenOnly = false;
		ChoiceBattle = Battle;
		ChoiceReal = Real;
		bTrying = false;
		ChoiceComing = Coming;
		ChoiceStage = 0;
		ChoiceO = 0;
		ChoiceV = 0;
		ChoiceTries = 0;
		ChoiceFrames = 0;
		bChoosing = true;
	}

	bool Director::ShareSpent(long Start, long Budget) const
	{
		if (RaySteps - Start >= Budget) return true;
		if (Set.ChooseMs <= 0.0 || Budget >= std::numeric_limits<long>::max() / 4) return false;
		// (some steps every frame whatever the clock, so a choice always goes on: the frame's other work alone may run
		// past the share on a slow machine)
		if (RaySteps - Start < Set.ChooseLeast) return false;
		return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - FrameClock).count() >= Set.ChooseMs;
	}

	bool Director::ScanChoice(long Start, long Budget)
	{
		// The men and tanks still up, by what is about to happen round them, most of all in the next two seconds (cut on
		// action: to what fires, bursts or falls next), as they were when the choice was begun; a frame's share of them
		// each frame, the next going on where it left off.
		const double Battle = ChoiceBattle, Real = ChoiceReal;
		const double SoonFall = Battle + Soon(0.5), SoonOut = Battle + Soon(4.0);
		auto Standing = [Battle, SoonFall, SoonOut](const Track& Tr)
		{
			return !Tr.bTwin && !(Tr.Fell >= 0.0 && Tr.Fell < SoonFall) && Tr.Start() <= Battle && !(Tr.Out >= 0.0 && Tr.Out < SoonOut);
		};
		for (; ChoiceScan < Rec->Tracks.size(); ++ChoiceScan)
		{
			if ((ChoiceScan & 15) == 0 ? ShareSpent(Start, Budget) : RaySteps - Start >= Budget) return false;
			const size_t I = ChoiceScan;
			const Track& Tr = Rec->Tracks[I];
			// (the scan's own walk over the tracks: a sixteenth of a ray's step each, in the frame's count, so a battle of
			// thousands of men takes it over more frames, not a longer one: CD0, Kursk's 5,800)
			if ((I & 15) == 0) ++RaySteps;
			if (!Standing(Tr)) continue;
			// (the cheap questions first, the side before the man's moves: the answers are as before)
			const uint8_t TrKind = KindOf(I);
			if (TrKind == KindLorry || TrKind == KindGun) continue;
			const LandFeed::Block* B = BlockOfTrack(static_cast<int>(I));
			if (!B) continue;
			const bool bTank = TrKind == KindTank;
			if (bScanTanksOnly && !bTank) continue;
			if (bScanMenOnly && TrKind != 0) continue;
			if (bScanSquadOnly && !InSquad(static_cast<int>(I))) continue;
			// (riding with a unit, or the general's order just given: its men alone, of whichever side)
			if (bScanRide && Tr.Block != ScanBlock) continue;
			if ((B->Side == Set.Own) != bScanOwnTurn && !bScanTanksOnly && !bScanSquadOnly && !bScanRide && ScanElsewhere == 0) continue;
			if (HiddenNow(static_cast<int>(I))) continue;
			// (nor a man riding in a lorry's bed: Sedan's motorised men sat nine to a lorry 0.6 m apart for 70 s)
			const LandFeed::Change* C = LandFeed::ChangeAt(Tr, Battle);
			if (C && C->Pose == "mounted") continue;
			const LandFeed::Spot S = LandFeed::SpotAt(Tr, Battle);
			// (weighed again: none near the men no place fit on)
			bool bAvoided = false;
			const double AvoidBy = bScanRide ? Set.ElsewhereRide : Set.Elsewhere;
			for (const Vec& A : ScanAvoid) bAvoided = bAvoided || std::hypot(S.X - A.X, S.Y - A.Y) < AvoidBy;
			if (bAvoided) continue;
			double Near = 0.0;
			long Seen = 0;
			const double Far = HeatNear(S.X, S.Y, Battle + Soon(0.5), Battle + Soon(8.0), 70.0, Battle + Soon(0.5), Battle + Soon(2.5), 40.0, Near, Seen);
			// (the heats looked at: about a twelfth of a ray's step each, and the man's own weighing two)
			RaySteps += 2 + Seen / 12;
			double Score = Near * 2.5 + Far;
			// Someone about to fall right here: the moment the film is for. Seen, not dwelt on.
			if (Tr.Fell >= Battle + Soon(1.0) && Tr.Fell < Battle + Soon(7.0)) Score += bScanFallsDue || Tr.IsVehicle() ? 12.0 : 3.0;
			// Moving men read best before the fight; men firing during it. Brian's moving tanks most of all.
			if (!bScanFighting && S.V > 0.3) Score += 3.0;
			if (C && C->bAim) Score += 2.0;
			if (bTank)
			{
				Score += (bScanFighting ? 1.5 : 2.5) + (S.V > 0.5 ? 1.0 : 0.0);
				// (in their turn the fight round a tank counts only so far: one on the move before one standing,
				// and one not shown lately before the one that was, so the film goes round the tanks of both sides)
				if (bScanTanksOnly) Score = std::min(Score, 40.0) + (S.V > 0.5 ? 25.0 : 0.0) - (Real - ShownAt[I] < 45.0 ? 30.0 : 0.0) + Roll() * 15.0
					+ (B->Side != LastTankSide ? 15.0 : 0.0); // (the other side's tanks than last time: the Shermans and the Germans in turn)
				// (the tanks get a shot in turn with the men: never two running on tanks unless one is about to die)
				else if (!Recent.empty() && (Recent.back() == Kind::Tank || Recent.back() == Kind::Track)) Score *= 0.4;
			}
			// (the platoon cam: the leading squad's men first, and in a rush the men on the move)
			if (LeadSide >= 0 && bScanRide && Tr.Block == LeadBlockNow)
				Score += (InLead(I, Battle) ? 6.0 : 0.0) + (InRush(I, Battle) ? 1.5 : 0.0);
			if (static_cast<int>(I) == Subject) Score *= 0.3; // not the same man twice running
			else if (!bTank && Real - ShownAt[I] < 20.0) Score *= 0.6;
			Score += Roll() * 2.5;
			for (int K = 0; K < 4; ++K)
			{
				if (Score <= ChoiceTop[K].Score) continue;
				for (int J = 3; J > K; --J) ChoiceTop[J] = ChoiceTop[J - 1];
				ChoiceTop[K] = { Score, static_cast<int>(I) };
				break;
			}
		}
		// (riding with a unit whose last men are going down or off the field now: let go, and the field weighed instead,
		// not the high view for want of a man)
		if (bScanRide && ChoiceTop[0].Who < 0 && ScanAvoid.empty())
		{
			if (ScanBlock == RideBlock) RideBlock = -1;
			bScanRide = false;
			ScanBlock = -1;
			ChoiceScan = 0;
			return ScanChoice(Start, Budget);
		}
		return true;
	}

	void Director::ChoiceOptions(double Battle)
	{
		// The best four's options, as they stand now: the kinds of shot that suit each, in turn.
		for (const Pick& P : ChoiceTop)
		{
			if (P.Who < 0) continue;
			const Track& Tr = Rec->Tracks[static_cast<size_t>(P.Who)];
			const LandFeed::Change* C = LandFeed::ChangeAt(Tr, Battle);
			const std::string Pose = C ? C->Pose : std::string("idle");
			const double Pick2 = Roll();
			Kind Order[3] = { Kind::Shoulder, Kind::Watch, Kind::Watch };
			if (Tr.Role == "tank")
			{
				const bool bMoving = LandFeed::SpotAt(Tr, Battle).V > 0.5;
				Order[0] = bMoving || Pick2 < 0.45 ? Kind::Track : Kind::Tank;
				Order[1] = Order[0] == Kind::Track ? Kind::Tank : Kind::Track;
				Order[2] = Kind::Watch;
			}
			else if (IsDown(Pose))
			{
				Order[0] = Pick2 < 0.55 ? Kind::Low : (Pick2 < 0.8 ? Kind::Shoulder : Kind::Watch);
				Order[1] = Order[0] == Kind::Low ? Kind::Shoulder : Kind::Low;
				Order[2] = Kind::Watch;
			}
			else
			{
				Order[0] = Pick2 < 0.7 ? Kind::Shoulder : Kind::Watch;
				Order[1] = Order[0] == Kind::Shoulder ? Kind::Watch : Kind::Shoulder;
				Order[2] = Kind::Low;
			}
			// Never the same kind three times running.
			if (Recent.size() >= 2 && Recent[Recent.size() - 1] == Order[0] && Recent[Recent.size() - 2] == Order[0]) std::swap(Order[0], Order[1]);
			// A man down in standing corn or tall grass: over his shoulder, not low among the stalks.
			Vec At;
			double Facing = 0.0;
			const bool bInCorn = WhereIs(P.Who, Battle, At, Facing) && CropAt(At.X, At.Y) > LowInCrop;
			for (const Kind K : Order)
			{
				if (K == Kind::Low && bInCorn) continue;
				// (in the fight a man is watched from beside him, at his height, more often than from a bank)
				Options.push_back({ K, P.Who, K == Kind::Watch && Tr.IsMan() && bScanFighting && Roll() < Set.NearWatch });
			}
		}
	}

	int Director::TryPlace(Kind K, int Who, double Battle, int Variant, long Start, long Budget)
	{
		if (!bTrying)
		{
			if (!Place(K, Who, Battle, Variant)) return 0;
			bTrying = true;
			TryPart = 0;
			TryBattle = Battle;
		}
		else
			PutShot(TryShot); // (the try's own place, from the frame before)
		// (its parts judged as of when it was placed, the next part left to the next frame once the share is spent)
		for (const int Parts = FitsParts(); TryPart < Parts; ++TryPart)
		{
			if (TryPart > 0 && ShareSpent(Start, Budget))
			{
				TryShot = KeepShot();
				return -1;
			}
			if (!FitsPart(TryBattle, TryPart))
			{
				bTrying = false;
				return 0;
			}
		}
		bTrying = false;
		return 1;
	}

	bool Director::TryChoice(double Battle, long Budget)
	{
		// Each option tried in turn, a few places each (the other side, farther, higher), until one is a fit picture:
		// the lens clear of things and men, the picture open. An asked shot keeps its kind even if none fits (the
		// picture run asks for that kind); with nothing fit, the high view (it always is).
		// (a frame's share of the weighing and the tries, by the ground asked under their rays and the heat looked at:
		// past it, they go on where they left off next frame, a try's parts too)
		const long Start = RaySteps;
		auto Spent = [&] { return ShareSpent(Start, Budget); };
		if (bScanning)
		{
			if (!ScanChoice(Start, Budget)) return false;
			bScanning = false;
			ChoiceOptions(Battle);
		}
		const bool bMiddle = bChoiceMiddle;
		if (ChoiceStage == 0)
		{
			for (; ChoiceO < Options.size() && ChoiceTries < TriesACut; ++ChoiceO, ChoiceV = 0)
			{
				const Option& Op = Options[ChoiceO];
				const int Variants = (Op.K == Kind::Wide || Op.K == Kind::Open) ? 1 : 4;
				const bool bAskedOp = bChoiceAsked && ChoiceO == 0;
				for (; ChoiceV < Variants && ChoiceTries < TriesACut; ++ChoiceV)
				{
					if (!bTrying && Spent()) return false;
					bNearWatch = Op.bNear;
					if (bAskedOp && !AskTries.empty()) { TryHouse = AskTries.front().House; TryAim = AskTries.front().Aim; }
					const int Fit = TryPlace(Op.K, Op.Who, Battle, ChoiceV, Start, Budget);
					if (Fit < 0) return false;
					if (Fit > 0) return true;
					++ChoiceTries;
				}
				if (bAskedOp)
				{
					// (the asked kind on the other men it may be of, two places each, before it is placed unfit)
					bNearWatch = false;
					for (; AskNext < AskTries.size(); ++AskNext, AskNextV = 0)
					{
						for (; AskNextV < 2; ++AskNextV)
						{
							TryHouse = AskTries[AskNext].House;
							TryAim = AskTries[AskNext].Aim;
							const int Fit = TryPlace(Op.K, AskTries[AskNext].Track, Battle, AskNextV, Start, Budget);
							if (Fit < 0) return false;
							if (Fit > 0) return true;
						}
					}
					TryHouse = AskTries.empty() ? -1 : AskTries.front().House;
					if (!AskTries.empty()) TryAim = AskTries.front().Aim;
					if (Place(Op.K, Op.Who, Battle, 0))
					{
						bAskedUnfit = true;
						TryHouse = -1;
						// (framed toward its house, but it keeps the house only if the house is in the picture)
						if (ShotHouse >= 0)
						{
							Vec E, L;
							Intended(Battle, E, L);
							if (!HouseSeenIn(E, L, Now.Fov, ShotHouse)) ShotHouse = -1;
						}
						return true;
					}
					TryHouse = -1;
				}
			}
			ChoiceStage = 1;
			ChoiceO = 0;
			ChoiceV = 0;
			ChoiceTries = 0;
		}
		// (none fit in the middle of the battle: the same again, a little less choosy (a place used before, a man not yet
		// in sight, more screened), before the high view: the art director's rule is no aerial in the middle of it)
		if (ChoiceStage == 1)
		{
			if (bMiddle)
			{
				bLoose = true;
				for (; ChoiceO < Options.size() && ChoiceTries < TriesACut / 3; ++ChoiceO, ChoiceV = 0)
				{
					const Option& Op = Options[ChoiceO];
					if (Op.K == Kind::Wide || Op.K == Kind::Open) continue;
					for (; ChoiceV < 2 && ChoiceTries < TriesACut / 3; ++ChoiceV)
					{
						if (!bTrying && Spent()) return false;
						bNearWatch = Op.bNear;
						const int Fit = TryPlace(Op.K, Op.Who, Battle, ChoiceV, Start, Budget);
						if (Fit < 0) return false;
						if (Fit > 0) return true;
						++ChoiceTries;
					}
				}
				bLoose = false;
			}
			ChoiceStage = 2;
			ChoiceO = 0;
			ChoiceV = 4;
		}
		// (and still none: from a bank or a rise farther back and higher on one of the men it was going to, a long lens,
		// where neither a man nor a hedge comes into the lens; the rescue of a shot falling to the aerial mid-fight came
		// and went with every small change of the field)
		if (ChoiceStage == 2)
		{
			// (the barrage's cut has no long lens from a bank in the hedges: framed toward the salvo or not taken, and the
			// film's own weighing after it framed as ever)
			const bool bFarBank = bMiddle && !bTrySalvo;
			bTrySalvo = false;
			if (bFarBank)
			{
				bLoose = true;
				bNearWatch = false;
				for (; ChoiceO < Options.size(); ++ChoiceO, ChoiceV = 4)
				{
					if (Options[ChoiceO].Who < 0) continue;
					for (; ChoiceV < 8; ++ChoiceV)
					{
						if (!bTrying && Spent()) return false;
						const int Fit = TryPlace(Kind::Watch, Options[ChoiceO].Who, Battle, ChoiceV, Start, Budget);
						if (Fit < 0) return false;
						if (Fit > 0) return true;
					}
				}
				for (size_t O = 0; O < Options.size(); ++O)
					if (Options[O].Who >= 0 && Place(Kind::Watch, Options[O].Who, Battle, 6)) return true;
				bLoose = false;
			}
			ChoiceStage = 3;
		}
		bNearWatch = false;
		// (riding with a unit, or going to the men the general just sent, and no place fit on any of them: first its other
		// men, away from those four (a unit of a hundred has more than four places to stand: Kursk's ride left its men 7 s
		// in 80), then the field weighed for this one cut instead, the ride kept, before the aerial: the ride over
		// Normandy's platoon battle went up to one in the middle of the fight)
		if (bMiddle && bScanRide && ScanElsewhere == 0 && Set.ElsewhereRide > 0.0)
		{
			++ScanElsewhere;
			for (const Pick& P : ChoiceTop)
			{
				Vec At;
				double Facing = 0.0;
				if (P.Who >= 0 && WhereIs(P.Who, Battle, At, Facing)) ScanAvoid.push_back(At);
			}
			if (!ScanAvoid.empty())
			{
				bScanning = true;
				ChoiceScan = 0;
				for (Pick& P : ChoiceTop) P = Pick();
				Options.clear();
				ChoiceStage = 0;
				ChoiceO = 0;
				ChoiceV = 0;
				ChoiceTries = 0;
				return false;
			}
		}
		if (bMiddle && bScanRide)
		{
			// (and then the field, its own elsewhere passes counted afresh, still away from the men tried)
			ScanElsewhere = 0;
			bScanRide = false;
			ScanBlock = -1;
			bScanning = true;
			ChoiceScan = 0;
			for (Pick& P : ChoiceTop) P = Pick();
			Options.clear();
			ChoiceStage = 0;
			ChoiceO = 0;
			ChoiceV = 0;
			ChoiceTries = 0;
			return false;
		}
		// (and still none, on men packed shoulder to shoulder or every place in a burst's cloud: the field weighed again,
		// both sides, for men away from these and those tried before, before the aerial; Sedan's army waiting in the woods
		// sent it up)
		if (bMiddle && ScanElsewhere < Set.ElsewhereTimes && Set.Elsewhere > 0.0)
		{
			++ScanElsewhere;
			bool bAllTanks = true, bAny = false;
			for (const Pick& P : ChoiceTop)
			{
				Vec At;
				double Facing = 0.0;
				if (P.Who >= 0 && WhereIs(P.Who, Battle, At, Facing)) ScanAvoid.push_back(At);
				if (P.Who >= 0)
				{
					bAny = true;
					bAllTanks = bAllTanks && Rec->Tracks[static_cast<size_t>(P.Who)].IsVehicle();
				}
			}
			// (every one of them a tank, in the woods: the men on foot instead, the tanks' turn let go)
			if (bAny && bAllTanks) bScanMenOnly = true;
			// (or none weighed at all, the turn's own men all riding or gone: the turn let go)
			if (!ScanAvoid.empty() || bScanTanksOnly || bScanSquadOnly)
			{
				bScanTanksOnly = false;
				bScanSquadOnly = false;
				bScanning = true;
				ChoiceScan = 0;
				for (Pick& P : ChoiceTop) P = Pick();
				Options.clear();
				ChoiceStage = 0;
				ChoiceO = 0;
				ChoiceV = 0;
				ChoiceTries = 0;
				return false;
			}
		}
		Place(Kind::Wide, -1, Battle);
		return true;
	}

	void Director::FinishChoice(double Battle, double Real)
	{
		const int Coming = ChoiceComing;
		bChoosing = false;
		bLoose = false;
		TryHouse = -1;
		bTrySalvo = false;
		bShotAsked = bChoiceAsked;
		if (Shot == Kind::Wide && Battle > Rec->End) bClosed = true;
		Recent.push_back(Shot);
		if (Recent.size() > 4) Recent.erase(Recent.begin());
		SinceSquad = InSquad(Subject) ? 0 : SinceSquad + 1;
		if (Shot == Kind::Shoulder || Shot == Kind::Low)
		{
			Vec E, L;
			Intended(Battle, E, L);
			UsedSpots.push_back(E);
		}
		if (Subject >= 0 && static_cast<size_t>(Subject) < ShownAt.size())
		{
			ShownAt[static_cast<size_t>(Subject)] = Real;
			const Track& Ts = Rec->Tracks[static_cast<size_t>(Subject)];
			const LandFeed::Block* Bs = BlockOfTrack(Subject);
			if (Ts.Role == "tank" && Bs) LastTankSide = Bs->Side;
		}
		ShotStarted = Real;
		BattleAtCut = Battle;
		NextLook = Real + 0.25;
		bLookAhead = false;
		bShoveCut = false;
		ScreenedSince = -1.0;
		BadSince = -1.0;
		BurstCutAt = -1.0;
		LostSince = -1.0;
		DarkSince = LightSince = -1.0;
		ShotLength = Set.ShotMin + Roll() * (Set.ShotMax - Set.ShotMin);
		if (Shot == Kind::Wide) ShotLength *= 0.7;
		// (under shelling the holds are short and nervous: heavy rounds landing round its man in its time)
		if (Subject >= 0 && Shot != Kind::Wide && Shot != Kind::Open)
		{
			Vec At;
			double Facing = 0.0;
			if (WhereIs(Subject, Battle, At, Facing) && HeavyNear(At.X, At.Y, Battle - Soon(1.0), Battle + Soon(ShotLength), 40.0) >= 2)
				ShotLength = std::max(Set.ShotLeast, ShotLength * Set.ShakenHold);
		}
		// (a fall gone to holds for it: on him, his own fall's hold then cuts; on a man beside him, until it is done)
		WaitFor = -1.0;
		if (Coming >= 0 && Shot != Kind::Wide)
		{
			const Track& Tc = Rec->Tracks[static_cast<size_t>(Coming)];
			WaitFor = Tc.Fell;
			ShotLength = std::max(ShotLength, OnScreen(Tc.Fell - Battle) + Set.FallHold);
			if (!Tc.IsVehicle()) LastFallCut = Real;
		}
		// (the whole mass holds its own length, and is taken once)
		if (bMassNow)
		{
			ShotLength = Set.MassLength;
			bMassDone = true;
			MassCutAt = Battle;
		}
		++CutCount;
		CutLog.emplace_back(Battle, Shot);
		Now.bCut = true;
	}

	bool Director::InSquad(int TrackIndex) const
	{
		if (!Rec || HeroBlock < 0 || TrackIndex < 0 || static_cast<size_t>(TrackIndex) >= Rec->Tracks.size()) return false;
		const Track& Tr = Rec->Tracks[static_cast<size_t>(TrackIndex)];
		return Tr.Block == HeroBlock && Tr.K == HeroK && KindOf(static_cast<size_t>(TrackIndex)) == 0;
	}

	bool Director::HiddenNow(int TrackIndex) const
	{
		if (!Rec || !Here.Hidden || TrackIndex < 0 || static_cast<size_t>(TrackIndex) >= Rec->Tracks.size()) return false;
		// (once a frame for each block: a scan over every man asked the scene 5,800 times a frame at Kursk)
		BlockOfTrack(TrackIndex);
		const int B = BlockIx[static_cast<size_t>(TrackIndex)];
		if (B < 0) return Here.Hidden(Rec->Tracks[static_cast<size_t>(TrackIndex)].Block);
		if (HiddenStamp.size() != Rec->Blocks.size())
		{
			HiddenStamp.assign(Rec->Blocks.size(), 0);
			HiddenVal.assign(Rec->Blocks.size(), 0);
		}
		if (HiddenStamp[static_cast<size_t>(B)] != FrameStamp)
		{
			HiddenStamp[static_cast<size_t>(B)] = FrameStamp;
			HiddenVal[static_cast<size_t>(B)] = Here.Hidden(Rec->Tracks[static_cast<size_t>(TrackIndex)].Block) ? 1 : 0;
		}
		return HiddenVal[static_cast<size_t>(B)] != 0;
	}

	int Director::HeavyNear(double X, double Y, double From, double To, double Radius) const
	{
		auto First = std::lower_bound(Heavy.begin(), Heavy.end(), From, [this](int I, double T) { return Rec->Rounds[static_cast<size_t>(I)].L < T; });
		int N = 0;
		for (auto It = First; It != Heavy.end(); ++It)
		{
			const LandFeed::Round& R = Rec->Rounds[static_cast<size_t>(*It)];
			if (R.L > To) break;
			if (std::hypot(R.X - X, R.Y - Y) < Radius) ++N;
		}
		return N;
	}

	void Director::MassWindow(double& OutFrom, double& OutTo) const
	{
		OutFrom = Set.MassFrom;
		OutTo = Set.MassTo;
		// (a battle too short for the window: the same share of it)
		if (Rec && !Rec->bLive && Rec->End < Set.MassTo + 15.0)
		{
			OutFrom = Rec->End * 0.55;
			OutTo = Rec->End * 0.72;
		}
	}

	bool Director::MassDue(double Battle, int Coming) const
	{
		if (bMassDone || Set.MassFrom <= 0.0 || RideBlock >= 0) return false;
		double From = 0.0, To = 0.0;
		MassWindow(From, To);
		if (Battle < From || Battle > To) return false;
		// (the window closing: now, whatever was coming)
		if (Battle >= To - Soon(MassClosing)) return true;
		if (Coming >= 0) return false;
		// (a tank about to be knocked out in its time is gone to first, as the film always goes to her, and the field
		// waits for a quieter moment)
		for (size_t I = 0; I < Rec->Tracks.size(); ++I)
		{
			const Track& Tr = Rec->Tracks[I];
			if (Tr.Fell >= Battle && Tr.Fell < Battle + Soon(Set.MassLength + 1.0) && Tr.IsVehicle() && !Tr.bTwin && !HiddenNow(static_cast<int>(I))) return false;
		}
		return true;
	}

	bool Director::PlaceMass(double Battle)
	{
		// The men drawn now and at the shot's end (every man on the field the player sees, the batteries far back and the
		// fallen where they lie), a few hundred of them weighed: every so many by the tracks' order, which runs through
		// each block's men in turn, so every block is weighed in its share.
		const double EndB = Battle + Soon(Set.MassLength);
		struct Man
		{
			Vec A, B;
			bool bA = false, bB = false;
			int Side = -1;
		};
		auto OnField = [](const Track& Tr, double T) { return Tr.Start() <= T && !(Tr.Out >= 0.0 && Tr.Out < T); };
		// (each track asked once: 1 drawn now, 2 at the end, 0 neither)
		std::vector<uint8_t> When(Rec->Tracks.size(), 0);
		size_t Drawn = 0;
		for (size_t I = 0; I < Rec->Tracks.size(); ++I)
		{
			const Track& Tr = Rec->Tracks[I];
			if (Tr.bTwin || !Tr.IsMan() || HiddenNow(static_cast<int>(I))) continue;
			When[I] = static_cast<uint8_t>((OnField(Tr, Battle) ? 1 : 0) | (OnField(Tr, EndB) ? 2 : 0));
			if (When[I]) ++Drawn;
		}
		const size_t Stride = std::max<size_t>(1, Drawn / 240);
		std::vector<Man> Men;
		Men.reserve(Drawn / Stride + 1);
		size_t Seen = 0;
		Vec C;
		int NA = 0;
		double Ox = 0.0, Oy = 0.0, Ex = 0.0, Ey = 0.0;
		int No = 0, Ne = 0;
		for (size_t I = 0; I < Rec->Tracks.size(); ++I)
		{
			const Track& Tr = Rec->Tracks[I];
			const bool bA = (When[I] & 1) != 0, bB = (When[I] & 2) != 0;
			if (!bA && !bB) continue;
			if (Seen++ % Stride != 0) continue;
			Man M;
			M.bA = bA;
			M.bB = bB;
			const LandFeed::Spot Sa = LandFeed::SpotAt(Tr, Battle), Sb = LandFeed::SpotAt(Tr, EndB);
			const double Z = HeightAt(Sa.X, Sa.Y) + 0.9;
			M.A = Vec(Sa.X, Sa.Y, Z);
			M.B = Vec(Sb.X, Sb.Y, Z);
			const LandFeed::Block* Bk = BlockOfTrack(static_cast<int>(I));
			M.Side = Bk ? Bk->Side : -1;
			Men.push_back(M);
			if (!bA) continue;
			C = C + M.A;
			++NA;
			if (M.Side == Set.Own) { Ox += M.A.X; Oy += M.A.Y; ++No; }
			else if (M.Side >= 0) { Ex += M.A.X; Ey += M.A.Y; ++Ne; }
		}
		if (NA < 2)
		{
			bMassDone = true;
			return false;
		}
		C = C * (1.0 / NA);
		// The own side's rear: from the enemy's men toward its own (the attack from the south when a side is missing).
		Vec Rear = No > 0 && Ne > 0 ? Vec(Ox / No - Ex / Ne, Oy / No - Ey / Ne, 0.0) : Vec(0.0, Set.Own == 0 ? -1.0 : 1.0, 0.0);
		if (Rear.Length() < 1.0) Rear = Vec(0.0, Set.Own == 0 ? -1.0 : 1.0, 0.0);
		const double RearWay = std::atan2(Rear.X, Rear.Y);
		// (how wide the men stand across that way, the middle nine tenths of them: where the distances start)
		std::vector<double> Wide;
		const Vec Along = Vec(Rear.Y, -Rear.X, 0.0).Unit();
		for (const Man& M : Men)
			if (M.bA) Wide.push_back((M.A - C).Dot(Along));
		std::sort(Wide.begin(), Wide.end());
		double Front = Wide[static_cast<size_t>(0.95 * (Wide.size() - 1))] - Wide[static_cast<size_t>(0.05 * (Wide.size() - 1))];
		// (a front wider than MassFront, Kursk's 2.7 km: no picture holds it with its men still seen (a man a pixel tall
		// at 1.3 km); the densest MassFront of it is the picture, the men in that stretch now weighed, its middle the
		// look's: the production lead, 22 November)
		bStretch = false;
		if (Front > Set.MassFront && Set.MassFront > 0.0)
		{
			size_t Best = 0, Lo = 0, Most = 0;
			for (size_t Hi = 0; Hi < Wide.size(); ++Hi)
			{
				while (Wide[Hi] - Wide[Lo] > Set.MassFront) ++Lo;
				if (Hi - Lo + 1 > Most) { Most = Hi - Lo + 1; Best = Lo; }
			}
			StretchAt = C;
			StretchAlong = Along;
			StretchFrom = Wide[Best];
			StretchTo = Wide[Best] + Set.MassFront;
			bStretch = true;
			std::vector<Man> Kept;
			Vec Kc;
			int Kn = 0;
			for (const Man& M : Men)
			{
				const double U = ((M.bA ? M.A : M.B) - C).Dot(Along);
				if (U < StretchFrom || U > StretchTo) continue;
				Kept.push_back(M);
				if (M.bA) { Kc = Kc + M.A; ++Kn; }
			}
			if (Kn >= 2)
			{
				Men.swap(Kept);
				C = Kc * (1.0 / Kn);
				Front = Set.MassFront;
			}
			else bStretch = false;
		}
		const double Least = std::max(150.0, 0.5 * Front / (std::tan(27.5 * Pi / 180.0) * 16.0 / 9.0));
		// A place's share of the men in its picture, now and at the shot's end (its eye drifted across as a wide shot's
		// does), the lesser of the two; its look set in the middle of where they stand in the picture.
		long Weighed = 0;
		std::vector<double> Us, Vs;
		Us.reserve(Men.size());
		Vs.reserve(Men.size());
		auto Basis = [](const Vec& Eye, const Vec& Look, Vec& OutFwd, Vec& OutRight, Vec& OutUp)
		{
			OutFwd = (Look - Eye).Unit();
			OutRight = Vec(OutFwd.Y, -OutFwd.X, 0.0);
			OutRight = OutRight.Length() < 1e-6 ? Vec(1.0, 0.0, 0.0) : OutRight.Unit();
			OutUp = Vec(OutRight.Y * OutFwd.Z - OutRight.Z * OutFwd.Y, OutRight.Z * OutFwd.X - OutRight.X * OutFwd.Z, OutRight.X * OutFwd.Y - OutRight.Y * OutFwd.X);
		};
		auto ShareOf = [&](const Vec& Eye, const Vec& Look, double Fov, bool bEnd)
		{
			Vec F, R, U;
			Basis(Eye, Look, F, R, U);
			const double TanV = std::tan(Fov * 0.5 * Pi / 180.0) * 0.92, TanH = TanV * 16.0 / 9.0;
			int In = 0, All = 0;
			for (const Man& M : Men)
			{
				if (!(bEnd ? M.bB : M.bA)) continue;
				++All;
				const Vec To = (bEnd ? M.B : M.A) - Eye;
				const double Depth = To.Dot(F);
				if (Depth < 0.5) continue;
				if (std::abs(To.Dot(R)) <= TanH * Depth && std::abs(To.Dot(U)) <= TanV * Depth) ++In;
			}
			Weighed += All;
			return All > 0 ? static_cast<double>(In) / All : 1.0;
		};
		auto Middle = [](std::vector<double>& V)
		{
			if (V.empty()) return 0.0;
			const size_t Lo = static_cast<size_t>(0.03 * (V.size() - 1)), Hi = static_cast<size_t>(0.97 * (V.size() - 1));
			std::nth_element(V.begin(), V.begin() + static_cast<std::ptrdiff_t>(Lo), V.end());
			const double A = V[Lo];
			std::nth_element(V.begin(), V.begin() + static_cast<std::ptrdiff_t>(Hi), V.end());
			return 0.5 * (A + V[Hi]);
		};
		// The places, nearest first: from the own side's rear, then round to either hand; low, then higher; the lens
		// at 45 degrees, then 55. The first holding MassShare is taken, else the one holding the most.
		static constexpr double Farther[] = { 0.8, 1.0, 1.25, 1.55, 1.95, 2.45, 3.1, 3.9 };
		static constexpr double Rises[] = { 25.0, 35.0, 45.0 };
		static constexpr double Rounds[] = { 0.0, 35.0, -35.0, 70.0, -70.0 };
		static constexpr double Lenses[] = { 45.0, 55.0 };
		double BestShare = -1.0, BestFov = 45.0;
		Vec BestEye, BestLook;
		bool bFound = false;
		for (const double Far : Farther)
		{
			const double Dist = Least * Far;
			for (const double Rise : Rises)
			{
				const double Over = Dist * std::tan(Rise * Pi / 180.0);
				if (Over > Set.MassHighest) continue;
				for (const double Round : Rounds)
				{
					const Vec Foot = C + Before(RearWay + Round * Pi / 180.0, Dist);
					Vec Eye(Foot.X, Foot.Y, std::max(C.Z + Over, HeightAt(Foot.X, Foot.Y) + 30.0));
					// (its look on the men's middle in the picture: their spread across and up and down from the way to C)
					Vec F, R, U;
					Basis(Eye, C, F, R, U);
					Us.clear();
					Vs.clear();
					for (const Man& M : Men)
					{
						if (!M.bA) continue;
						const Vec To = M.A - Eye;
						const double Depth = std::max(0.5, To.Dot(F));
						Us.push_back(To.Dot(R) / Depth);
						Vs.push_back(To.Dot(U) / Depth);
					}
					Weighed += static_cast<long>(Us.size());
					const double Mu = Middle(Us), Mv = Middle(Vs);
					const Vec Look = Eye + (F + R * Mu + U * Mv) * (C - Eye).Length();
					const Vec Way = Look - Eye;
					const Vec Drift = Vec(Way.Y, -Way.X, 0.0).Unit() * (WideDrift * Set.MassLength);
					for (const double Fov : Lenses)
					{
						const double Share = std::min(ShareOf(Eye, Look, Fov, false), ShareOf(Eye + Drift, Look, Fov, true));
						if (Share > BestShare + 1e-9)
						{
							BestShare = Share;
							BestEye = Eye;
							BestLook = Look;
							BestFov = Fov;
						}
						if (Share >= Set.MassShare)
						{
							bFound = true;
							break;
						}
					}
					if (bFound) break;
				}
				if (bFound) break;
			}
			if (bFound) break;
		}
		// (the men weighed in the pictures, about a sixty-fourth of a ray's step each, and every man asked whether he is
		// drawn, about a thirty-second, in the choice's count)
		RaySteps += Weighed / 64 + static_cast<long>(Rec->Tracks.size() / 32);
		if (BestShare < 0.0)
		{
			bMassDone = true;
			return false;
		}
		Stand = KeepAbove(BestEye, 30.0);
		Target = BestLook;
		Now.Fov = BestFov;
		return true;
	}

	Pose Director::Update(double Battle, double Real, double RealDt)
	{
		Now.bCut = false;
		if (!Rec) return Now;
		FrameClock = std::chrono::steady_clock::now();
		// (the scene's answers asked once a frame, such as which blocks are hidden, are asked afresh)
		++FrameStamp;
		// (the frame's work so far, by the ground asked under the camera's rays: a cut's choice has what is left of the
		// frame's share)
		const long FrameFrom = RaySteps;
		// (a live battle grown since the last frame: its new tracks, rounds and knocked-out vehicles put in the lists)
		if (Rec->bLive) KeepUp(Battle);
		// (the platoon cam: the platoon leading the assault now)
		LookForLead(Battle, Real);
		IndexLandings();
		const double PrevBattle = LastBattle;
		// (the rounds that may still be in the air: none fired more than a minute before; a seek back starts again)
		if (Battle < LastBattle) RoundFrom = 0;
		while (RoundFrom < Rec->Rounds.size() && Rec->Rounds[RoundFrom].T < Battle - 60.0) ++RoundFrom;
		if (LastBattle >= 0.0 && Battle > LastBattle && ActionIn(Now, LastBattle, Battle)) LastAction = Real;
		// (the battle's pace on the screen, eased over a third of a second: a seek or a pause is not a pace)
		if (LastBattle >= 0.0 && RealDt > 1e-6 && Battle >= LastBattle && Battle - LastBattle < 2.0)
		{
			const double Now_ = (Battle - LastBattle) / RealDt;
			Pace = bPaced ? Pace + (Now_ - Pace) * (1.0 - std::exp(-RealDt / 0.3)) : Now_;
			bPaced = true;
		}
		LastBattle = Battle;

		const double Held = Real - ShotStarted;
		const double Contact = Rec->Contact >= 0.0 ? Rec->Contact : 0.0;
		const bool bFighting = Battle >= Contact - 5.0;
		bool bCut = bCutAsked;
		Why Reason = Why::Asked;
		bCutAsked = false;
		// (a cut being chosen over a few frames: the shot on the screen holds meanwhile and nothing else is weighed; an
		// asked shot drops the choice in hand and is chosen at once)
		if (bChoosing && bCut) bChoosing = false;
		// (a shot holding for a fall or a tank's end it shows is not left as dead or lost before it comes: a track shot
		// on a tank that had stopped was left as dead 0.2 s before she was knocked out)
		// (for a moment only: a shot waiting for a tank's end let its own man go lost from the picture for 5 s)
		const bool bWaiting = WaitFor >= 0.0 && Battle < WaitFor + 0.2 && WaitFor - Battle < Soon(2.5);
		// (its man gone into hiding from the player, an ambush he has not found: the film does not give it away)
		if (!bChoosing && !bCut && Subject >= 0 && HiddenNow(Subject))
		{
			bCut = true;
			Reason = Why::Gone;
		}
		// (the general's order: its lean lapses LeanFor after it is first seen, and is met by a shot already on its men)
		if (LeanBlock >= 0)
		{
			if (LeanUntil < 0.0) LeanUntil = Real + Set.LeanFor;
			if ((!bChoosing && Real > LeanUntil) || RideBlock >= 0 || (Subject >= 0 && Rec->Tracks[static_cast<size_t>(Subject)].Block == LeanBlock))
			{
				LeanBlock = -1;
				LeanUntil = -1.0;
			}
		}
		if (!bChoosing && !bHeld && !bCut)
		{
			if (Shot == Kind::Open && Held > Set.Opening) { bCut = true; Reason = Why::Opening; }
			else if (Shot != Kind::Open && Held > ShotLength && FellAt < 0.0) { bCut = true; Reason = Why::Length; }
			// The general's order (Lean): the shot left once it has run its least, for the men he sent.
			else if (LeanBlock >= 0 && Shot != Kind::Open && !bMassNow && FellAt < 0.0 && !bWaiting && Held >= Set.ShotLeast) { bCut = true; Reason = Why::Asked; }
			// Nothing has happened in it for a while, now that the armies are near: leave it.
			// (a tank on the move is the action of its own shot: Brian's moving tanks are watched as they go)
			else if (Shot != Kind::Open && Shot != Kind::Wide && bFighting && FellAt < 0.0 && !bWaiting && Held >= Set.ShotLeast &&
				Real - std::max(LastAction, ShotStarted) >= Set.DeadAfter && !TankMoving(Battle))
			{
				bCut = true;
				Reason = Why::Dead;
			}
			// Before they meet, the advance is the action: a shot in which nothing moves or fires is left too (the second
			// cut held 8.3 s on a parked tank looking into an orchard).
			else if (Shot != Kind::Open && Shot != Kind::Wide && !bFighting && FellAt < 0.0 && !bWaiting && Held >= Set.ShotLeast)
			{
				if (Real >= NextMoveLook)
				{
					NextMoveLook = Real + 0.25;
					if (MovingIn(Battle)) LastMoved = Real;
				}
				if (Real - std::max(LastMoved, std::max(LastAction, ShotStarted)) >= (Shot == Kind::Tank ? Set.DeadAfter : Set.QuietAfter))
				{
					bCut = true;
					Reason = Why::Dead;
				}
			}
		}
		// His fall: kept in the picture to the ground and held where he lies, then cut (bodies stay; the film moves on).
		if (Subject >= 0)
		{
			const Track& Tr = Rec->Tracks[static_cast<size_t>(Subject)];
			if (Tr.Fell >= 0.0 && Tr.Fell <= Battle && FellAt < 0.0)
			{
				FellAt = Real;
				FallEye = EyeEased;
				bFallEye = true;
			}
			if (!bChoosing && !bCut && FellAt >= 0.0 && Real > FellAt + Set.FallHold && !bHeld) { bCut = true; Reason = Why::Fall; }
			if (!bChoosing && !bCut && Tr.Out >= 0.0 && Battle > Tr.Out) { bCut = true; Reason = Why::Gone; }
		}
		// Cut on action: a tank about to be knocked out, or a man about to fall, a second or two ahead and not in this
		// picture: go to it now, just before it happens. One already in the picture: hold for it.
		// (the tanks first, then the men; a tank's end is gone to even from a man's fall held a second and a half)
		// (a tank's end is let through a man's fall held 0.8 s, and gone to as late as 0.4 s before it: the camera, held on a
		// man's fall, missed a tank knocked out 1.6 s after him, her window gone by)
		// (not from the whole mass: the field is its subject, and a tank's end or a fall is in it)
		if (!bChoosing && !bCut && !bHeld && Shot != Kind::Open && !bMassNow && (FellAt < 0.0 || Real > FellAt + Set.FallSeen) && Held >= Set.ShotLeast)
		{
			for (size_t N = 0; N < 2 * Rec->Tracks.size() && !bCut; ++N)
			{
				const bool bVehicles = N < Rec->Tracks.size();
				const size_t I = bVehicles ? N : N - Rec->Tracks.size();
				const Track& Tr = Rec->Tracks[I];
				if (Tr.Fell < Battle + Soon(bVehicles ? 0.4 : 1.0) || Tr.Fell > Battle + Soon(2.2)) continue;
				if (IsVehicleAt(I) != bVehicles || (!bVehicles && FellAt >= 0.0) || Tr.bTwin) continue;
				if (HiddenNow(static_cast<int>(I)) || OffRide(static_cast<int>(I))) continue;
				if (std::find(Appointed.begin(), Appointed.end(), static_cast<int>(I)) != Appointed.end()) continue;
				Appointed.push_back(static_cast<int>(I));
				if (!Tr.IsVehicle() && (Real - LastFallCut < 6.0 || Roll() > 0.75)) continue;
				// (a shot on a tank, the scarcer kind among hundreds of men, is not left for a man's fall before 6 s)
				if (!Tr.IsVehicle() && (Shot == Kind::Tank || Shot == Kind::Track) && Held < 6.0) continue;
				const LandFeed::Spot S = LandFeed::SpotAt(Tr, Battle);
				const Vec At(S.X, S.Y, HeightAt(S.X, S.Y) + 1.0);
				if (InPicture(Now, At, 0.9) && (At - Now.Eye).Length() < (Now.Fov < 30.0 ? 250.0 : 120.0))
				{
					ShotLength = std::max(ShotLength, Held + OnScreen(Tr.Fell - Battle) + Set.FallHold);
					WaitFor = std::max(WaitFor, Tr.Fell);
					continue;
				}
				Pending = static_cast<int>(I);
				bCut = true;
				Reason = Why::Action;
				break;
			}
		}
		// Radio call, then the barrage (Brian's release 2 ask): a fresh call for fire coming, the film cuts to the man calling
		// it as he begins (once in RadioEvery); then, his guns' first salvo RadioLead from landing, to the men under it,
		// unless the shot on the screen already shows the spot near, which then holds for it.
		IndexRadios();
		if (RadioStage == 2 && Battle > Radios[static_cast<size_t>(RadioNow)].Land + Soon(0.5)) RadioStage = 0;
		if (!Radios.empty() && !bChoosing && !bCut && !bHeld && Shot != Kind::Open && !bMassNow && !bWaiting && !bShotAsked && (FellAt < 0.0 || Real > FellAt + Set.FallSeen))
		{
			if (RadioStage == 0 && Real - LastRadioReal >= Set.RadioEvery && Held >= Set.ShotLeast)
			{
				while (RadioNext < Radios.size() && Radios[RadioNext].T < Battle - Soon(1.0)) ++RadioNext;
				if (RadioNext < Radios.size())
				{
					const Radio& Rd = Radios[RadioNext];
					const Track& Tc = Rec->Tracks[static_cast<size_t>(Rd.Caller)];
					const bool bUp = Tc.Start() <= Battle && !(Tc.Fell >= 0.0 && Tc.Fell < Rd.Land) && !(Tc.Out >= 0.0 && Tc.Out < Rd.Land);
					if (Battle >= Rd.T - Soon(0.5) && Rd.Land - Battle >= Soon(Set.ShotLeast + Set.RadioLead + 0.3) && bUp && !HiddenNow(Rd.Caller) && !OffRide(Rd.Caller))
					{
						RadioNow = static_cast<int>(RadioNext++);
						RadioStage = 1;
						bCut = true;
						Reason = Why::Action;
					}
				}
			}
			else if (RadioStage == 2)
			{
				const Radio& Rd = Radios[static_cast<size_t>(RadioNow)];
				if (Battle >= Rd.Land - Soon(Set.RadioLead) && Held >= Set.ShotLeast)
				{
					const Vec At(Rd.X, Rd.Y, HeightAt(Rd.X, Rd.Y) + 1.0);
					if (Shot != Kind::Wide && InPicture(Now, At, 0.8) && (At - Now.Eye).Length() < (Now.Fov < 30.0 ? 400.0 : 200.0))
					{
						RadioStage = 0;
						WaitFor = std::max(WaitFor, Rd.Land);
						ShotLength = std::max(ShotLength, Held + OnScreen(Rd.Land - Battle) + 2.5);
						if (!Beats.empty()) Beats.back().bBarrage = true;
					}
					else
					{
						bCut = true;
						Reason = Why::Action;
					}
				}
			}
		}
		// A shell landing at the lens (in the picture within a few metres, or right by it): cut on its flash, as a war film
		// cuts on an impact, not 0.8 s of dirt and dark smoke filling the picture (the second cut's Tiger shot).
		if (!bChoosing && Shot != Kind::Open && Shot != Kind::Wide)
		{
			if (BurstCutAt < 0.0)
			{
				double When = -1.0;
				if (LensBurst(Now.Eye, Now.Look, Now.Fov, Battle - Soon(0.05), Battle + Soon(0.3), Subject, &When))
					BurstCutAt = Real + OnScreen(std::max(0.0, When - Battle)) + Set.BurstFlash;
			}
			if (!bCut && !bHeld && BurstCutAt >= 0.0 && Real >= BurstCutAt)
			{
				bCut = true;
				Reason = Why::Burst;
			}
		}
		// Something come in front of the lens (a tank turning its gun across it, a man walking into it, the eye drifted
		// into a hedge): seen twice running, a quarter of a second apart, the shot is left at once.
		// (the look over two frames while the camera's work is spread: the picture now, then where it will be and the men
		// coming; a long-lens track at Kursk asked 4,500 ray steps of one frame. A picture already bad goes straight on.)
		const bool bLookOn = !bChoosing && !bCut && Here.Solid && Shot != Kind::Open && Shot != Kind::Wide;
		if (!bLookOn) bLookAhead = false;
		bool bLookDue = bLookOn && (bLookAhead || Real >= NextLook), bBad = false;
		if (bLookDue && !bLookAhead)
		{
			NextLook = Real + 0.25;
			// (a following eye run into a burst's cloud still hanging is left too)
			bBad = !RoomAt(Now.Eye, Subject) || BlockedShare(Now.Eye, Now.Look, Now.Fov, Subject, Set.BlockedMost) > Set.BlockedMost;
			const double Screened = bBad ? 1.0 : ScreenShare(Now.Eye, Now.Look, Now.Fov, Subject, Set.ScreenLeave);
			bBad = bBad || Screened > Set.ScreenLeave || InCloud(Now.Eye, Battle);
			// (screened over a quarter for ScreenStrain once it has held its least: left, not ridden on into the trees)
			if (Screened <= Set.ScreenMost) ScreenedSince = -1.0;
			else if (ScreenedSince < 0.0) ScreenedSince = Real;
			if (!bBad && !bHeld && ScreenedSince >= 0.0 && Real - ScreenedSince >= Set.ScreenStrain && Held >= Set.ShotLeast)
			{
				bCut = true;
				Reason = Why::Rescue;
			}
			if (!bBad && Set.ChooseSteps > 0)
			{
				bLookAhead = true;
				bLookDue = false;
			}
		}
		else if (bLookDue) bLookAhead = false;
		if (bLookDue)
		{
			// (where a following eye will be a moment on: left before it drives into a hedge, not once it is in it)
			if (!bBad && FellAt < 0.0 && (Shot == Kind::Track || Shot == Kind::Tank || Shot == Kind::Shoulder || Shot == Kind::Low))
			{
				Vec E, L;
				Intended(Battle + Soon(Set.AheadLook), E, L);
				bBad = BadPicture(E, L, Now.Fov, Set.ScreenLeave);
			}
			// (its own man filling the picture; a man of the recording about to walk into the lens, where it will be: that
			// one ends a fall's hold too, once the man is seen going down)
			if (!bBad && FellAt < 0.0 && (Shot == Kind::Shoulder || Shot == Kind::Low))
				bBad = OwnShare(Now.Eye, Now.Look, Now.Fov, Subject) > Set.OwnMost;
			if (!bBad && (FellAt < 0.0 || Real > FellAt + Set.FallSeen))
			{
				Vec Eyes[3];
				double Times[3];
				for (int K = 0; K < 3; ++K)
				{
					Times[K] = Battle + Soon(0.5 * K);
					Vec Lk;
					if (K == 0) Eyes[K] = Now.Eye;
					else Intended(Times[K], Eyes[K], Lk);
				}
				bBad = Crossing(Eyes, Times, 3, Subject);
			}
			// Its own man out of the picture, or down in the corn out of sight, for a second: left once it has run its least.
			if (FellAt < 0.0 && Shot != Kind::Tank && Shot != Kind::Track && OwnLost(Battle))
			{
				if (LostSince < 0.0) LostSince = Real;
			}
			else LostSince = -1.0;
			if (!bBad && !bHeld && !bWaiting && LostSince >= 0.0 && Real - LostSince >= Set.LostAfter && Held >= Set.ShotLeast)
			{
				bCut = true;
				Reason = Why::Lost;
			}
			if (!bBad) BadSince = -1.0;
			else if (BadSince < 0.0) BadSince = Real;
			else if (!bHeld && !bCut)
			{
				bCut = true;
				Reason = Why::Rescue;
			}
		}
		// The picture drawn too dark (the art director's rule, cut 4 fix 6): over DarkLeave under L 25 for DarkHold seconds
		// past its first quarter second, the shot is left, and the next stands clear of where it stood (a hedge's shaded
		// face, a crown's shade). After DarkRunMost such cuts running, the light is to blame, not the place: it rests,
		// until a picture has read no darker than DarkLight for as long, which shows that the light is not.
		if (!bChoosing && Here.DarkNow && Shot != Kind::Open && Shot != Kind::Wide)
		{
			const double Dark = Here.DarkNow();
			if (Dark <= Set.DarkLeave || Held < 0.25) DarkSince = -1.0;
			else if (DarkSince < 0.0) DarkSince = Real;
			// (light as plainly as dark is dark: a reading there is, no darker than DarkLight, for DarkHold seconds on end
			// past the shot's first quarter second. One light reading is not enough. The scene's reading is of a frame
			// asked for up to a quarter of a second ago and come back later still, so the first a shot is given may be
			// the last shot's picture, an opening's or a wide's among them, which this rule never judges; and one frame
			// may be a shell's flash. A reading that stands half a second is this shot's own, and no flash.)
			if (Dark < 0.0 || Dark > Set.DarkLight || Held < 0.25) LightSince = -1.0;
			else if (LightSince < 0.0) LightSince = Real;
			if (LightSince >= 0.0 && Real - LightSince >= Set.DarkHold) DarkRunNow = 0;
			if (!bCut && !bHeld && DarkSince >= 0.0 && Real - DarkSince >= Set.DarkHold && DarkRunNow < Set.DarkRunMost)
			{
				bCut = true;
				Reason = Why::Dark;
				++DarkLeft;
				++DarkRunNow;
				DarkSpots.push_back(Now.Eye);
				if (DarkSpots.size() > 32) DarkSpots.erase(DarkSpots.begin());
			}
		}
		// Its own man come into the lens at once (a man lying down turned about where he lay, and his head was a hand from
		// the lens; a crawling man's track hopping 2.4 m at Prokhorovka, the lens shoved 0.47 m to keep clear): no operator
		// steps a metre in a frame; the film cuts away. (asked where the keep-clear asks it: the eased eye, under the lift)
		if (!bChoosing && !bCut && !bHeld && FellAt < 0.0 && (Shot == Kind::Shoulder || Shot == Kind::Low) &&
			(OwnDistance(Now.Eye, Subject, Battle) < Set.OwnClear * Set.OwnCutInside ||
				Flat(ClearOfOwn(EyeEased + OwnPush, Battle, Set.OwnClear * Set.OwnNearest), EyeEased + OwnPush) > Set.OwnShoveMost))
		{
			bCut = true;
			bShoveCut = true;
			Reason = Why::Rescue;
		}
		// The whole mass's window closing with it not yet taken: the shot on the screen is left for it once it has held its
		// least (MassDue takes it then, whatever was coming).
		if (!bChoosing && !bCut && !bHeld && !bMassDone && Set.MassFrom > 0.0 && RideBlock < 0 && Held >= Set.ShotLeast)
		{
			double From = 0.0, To = 0.0;
			MassWindow(From, To);
			if (Battle >= To - Soon(MassClosing) && Battle <= To)
			{
				bCut = true;
				Reason = Why::Length;
			}
		}
		if (bCut || bChoosing)
		{
			// (the cut's choice: begun now, or going on from the frames before; this frame's share of its tries, the
			// shot on the screen kept to be put back if none is placed yet)
			if (!bChoosing)
			{
				// (an asked still left before it can have been taken: asked again, its kind on another man or place, so the
				// picture run's still is of what it asked: Sedan's 03 ran its man's eye into a tree a second after the ask)
				if (bShotAsked && Reason != Why::Asked && Real - ShotStarted < Set.AskHold && AskAgain < Set.AskAgainMost)
				{
					++AskAgain;
					AskLeft.push_back(Now.Eye);
					bAsked = true;
				}
				LeftWhy = Reason;
				BeginChoice(Battle, Real);
			}
			const bool bAtOnce = bChoiceAsked || Set.ChooseSteps <= 0 || (Subject >= 0 && HiddenNow(Subject));
			const ShotKept Was = KeepShot();
			if (ChoiceFrames > 0) VariantSide = ChoiceSide;
			const long Budget = bAtOnce ? std::numeric_limits<long>::max() / 2 : std::max<long>(0, Set.ChooseSteps - (RaySteps - FrameFrom));
			++ChoiceFrames;
			if (TryChoice(Battle, Budget))
			{
				ChoiceFramesMost = std::max(ChoiceFramesMost, ChoiceFrames);
				FinishChoice(Battle, Real);
				// (radio call, then the barrage: on the caller, his guns' salvo to come; on the men under it, held for it)
				if (RadioStage == 1)
				{
					const Radio& Rd = Radios[static_cast<size_t>(RadioNow)];
					RadioStage = bRadioChoice && Subject == Rd.Caller ? 2 : 0;
					if (RadioStage == 2)
					{
						LastRadioReal = Real;
						Beats.push_back({ Rd.T, Rd.Land, Rd.X, Rd.Y, Rd.Caller, false });
					}
				}
				else if (RadioStage == 2 && bRadioChoice)
				{
					// (placed framed toward the salvo: held for it; else the film's own shot, the barrage let go)
					const Radio& Rd = Radios[static_cast<size_t>(RadioNow)];
					RadioStage = 0;
					if (bShotSalvo)
					{
						WaitFor = std::max(WaitFor, Rd.Land);
						ShotLength = std::max(ShotLength, OnScreen(Rd.Land - Battle) + 2.5);
						if (!Beats.empty()) Beats.back().bBarrage = true;
					}
				}
				bRadioChoice = false;
			}
			else
			{
				ChoiceSide = VariantSide;
				PutShot(Was);
			}
		}

		Vec WantEye = Stand, WantLook = Target;
		const double ShotHeld = Real - ShotStarted;
		if (Shot == Kind::Open)
		{
			// Coming down over the attack toward the field: 170 m to 70 m over the opening.
			const double U = std::clamp(ShotHeld / std::max(1.0, Set.Opening), 0.0, 1.0);
			const double S = U * U * (3.0 - 2.0 * U);
			const Vec Way = (Target - Stand);
			WantEye = Stand + Vec(Way.X, Way.Y, 0.0) * (0.28 * S);
			WantEye.Z = Stand.Z - 100.0 * S;
		}
		else if (Shot == Kind::Wide)
		{
			// A slow drift across, as from a crane.
			const Vec Way = (Target - Stand);
			const Vec Across = Vec(Way.Y, -Way.X, 0.0).Unit();
			WantEye = Stand + Across * (ShotHeld * WideDrift);
		}
		else if (Subject >= 0)
		{
			Vec At;
			double Facing = 0.0;
			WhereIs(Subject, Battle, At, Facing);
			// The operator turns with him, not at once, and never whips round (a man turning to fire behind him turns in
			// the picture; the second cut's shoulder shots swung round at 160 degrees a second).
			{
				const double Was = FacingEased;
				FacingEased = LandFeed::TurnToward(FacingEased, Facing, 1.0 - std::exp(-RealDt / 1.1));
				const double Most = Set.TurnMost * Pi / 180.0 * RealDt;
				const double Turned = LandFeed::Wrap(FacingEased - Was);
				if (std::abs(Turned) > Most) FacingEased = LandFeed::Wrap(Was + (Turned > 0.0 ? Most : -Most));
			}
			Vec BodyAt;
			const bool bBody = BodyOf(Subject, Battle, BodyAt);
			if (Shot == Kind::Watch)
			{
				WantLook = bBody ? BodyAt : At + Vec(0.0, 0.0, 1.0);
				// (the village still and the barrage's cut keep the look turned toward their house or salvo as placed, not
				// eased back onto the man alone: a salvo framed at the picture's edge slid out of it as it landed)
				if (ShotHouse >= 0 || bShotSalvo) WantLook = WantLook + (ShotAim - WantLook) * AimLook();
			}
			else if (Shot == Kind::Track)
			{
				// Kept off her side as she drives (a car alongside), the lens a little ahead of her: she drives into it.
				const LandFeed::Spot S = LandFeed::SpotAt(Rec->Tracks[static_cast<size_t>(Subject)], Battle);
				WantEye = At + TrackOff;
				WantEye.Z = HeightAt(WantEye.X, WantEye.Y) + TrackUp;
				WantLook = At + Before(S.Way, std::min(S.V, 6.0) * 0.5) + Vec(0.0, 0.0, 1.3);
				if (FellAt >= 0.0 && bBody) WantLook = BodyAt;
			}
			else
			{
				// (over the shoulder, the operator stands, kneels and lies down with him)
				const double UpNow = Shot == Kind::Shoulder ? ShoulderUp(Battle) + UpJitter : Up;
				WantEye = At + Offset(FacingEased, Back, Side) + Vec(0.0, 0.0, UpNow);
				const double LookFar = Shot == Kind::Tank ? 70.0 : 35.0;
				WantLook = At + Before(FacingEased, LookFar);
				WantLook.Z = HeightAt(WantLook.X, WantLook.Y) + LookOver(UpNow);
				// (never in his face: as he turns or rises the lens is kept OwnClear from his head and chest)
				if (FellAt < 0.0 && Shot != Kind::Tank) WantEye = ClearOfOwn(WantEye, Battle);
				if (FellAt >= 0.0)
				{
					// (he is hit: the eye stops where it was and the lens follows his body down, kept in the picture to the
					// ground, never nearer him than a couple of metres even if he is thrown toward it)
					if (bFallEye) WantEye = FallEye;
					WantLook = bBody ? BodyAt : At + Vec(0.0, 0.0, 0.2);
					const double Gap = Flat(WantEye, WantLook);
					if (Gap < 2.0)
					{
						Vec Away = WantEye - WantLook;
						Away.Z = 0.0;
						Away = Away.Length() > 1e-6 ? Away.Unit() : Before(FacingEased + Pi, 1.0);
						WantEye = WantEye + Away * (2.0 - Gap);
						WantEye.Z = std::max(WantEye.Z, WantLook.Z + 0.6);
					}
				}
			}
			if (Here.Free && !Here.Free(WantEye))
			{
				// Inside a bank or a hedge: step the eye up and in toward him until it is clear.
				for (int Try = 1; Try <= 4 && !Here.Free(WantEye); ++Try)
					WantEye = WantEye + (At + Vec(0.0, 0.0, Up) - WantEye) * 0.3 + Vec(0.0, 0.0, 0.3);
			}
		}
		if (Now.bCut)
		{
			EyeEased = WantEye;
			LookEased = WantLook;
			LookSwing = Vec();
		}
		else
		{
			const bool bFollow = Shot == Kind::Shoulder || Shot == Kind::Low || Shot == Kind::Tank;
			const double EyeEase = bFollow ? 0.35 : Shot == Kind::Track ? 0.5 : 0.8;
			const double LookEase = (bFollow || Shot == Kind::Track) ? 0.25 : 0.6;
			EyeEased = Ease(EyeEased, WantEye, EyeEase, RealDt);
			// The look on a spring, damped just enough not to overshoot: a pan starts and stops without a kick (the second
			// cut's lens whipped 62 pixels in a frame onto a man as he was hit, then died away), and it follows a steady
			// move as the old ease did (its lag 2/W, the old ease's time). On a fall, LookRate: about a third of a second.
			const double W = FellAt >= 0.0 ? Set.LookRate : 2.0 / LookEase;
			const Vec Off = LookEased - WantLook;
			const Vec Mix = LookSwing + Off * W;
			const double Decay = std::exp(-W * RealDt);
			LookEased = WantLook + (Off + Mix * RealDt) * Decay;
			LookSwing = (LookSwing - Mix * (W * RealDt)) * Decay;
		}
		// The hand-held wobble: a few slow sines, stronger on the ground among the men, slight from a crane.
		// (the operator's nerves: a heavy round bursting near the lens shakes the hand harder for a moment, more the
		// nearer it was; up over a few frames, settling over two seconds)
		if (PrevBattle >= 0.0 && Battle > PrevBattle && Shot != Kind::Open && Shot != Kind::Wide)
		{
			auto First = std::upper_bound(Heavy.begin(), Heavy.end(), PrevBattle, [this](double T, int I) { return T < Rec->Rounds[static_cast<size_t>(I)].L; });
			for (auto It = First; It != Heavy.end(); ++It)
			{
				const LandFeed::Round& R = Rec->Rounds[static_cast<size_t>(*It)];
				if (R.L > Battle) break;
				const double Gap = std::hypot(R.X - Now.Eye.X, R.Y - Now.Eye.Y);
				if (Gap < Set.NervesNear) NervesWant = std::min(1.0, NervesWant + 0.8 * (1.0 - Gap / Set.NervesNear));
			}
		}
		NervesWant *= std::exp(-RealDt / std::max(0.1, Set.NervesFade));
		Nerves = NervesWant > Nerves ? Nerves + (NervesWant - Nerves) * (1.0 - std::exp(-RealDt / 0.15)) : NervesWant;
		const double Hand = Set.Hand * (1.0 + Set.NervesMost * Nerves) *
			(Shot == Kind::Shoulder || Shot == Kind::Low ? 1.0 : Shot == Kind::Tank ? 0.8 : Shot == Kind::Watch || Shot == Kind::Track ? 0.4 : 0.15);
		auto Wave = [&](int K, double Hz) { return std::sin(Real * Hz * 2.0 * Pi + Phase[K]); };
		const Vec Wobble = Vec(Wave(0, 0.37) + 0.5 * Wave(1, 1.13), Wave(2, 0.29) + 0.5 * Wave(3, 0.97), Wave(4, 0.41) + 0.4 * Wave(5, 1.31)) * (0.035 * Hand);
		const double LookFar = (LookEased - EyeEased).Length();
		const Vec LookWobble = Vec(Wave(1, 0.23), Wave(3, 0.31), Wave(5, 0.19)) * (LookFar * 0.006 * Hand);
		Now.Eye = EyeEased + Wobble;
		// (the eased eye lags the one wanted: it too is kept out of its own man's face, the push on a stiff spring, as an
		// operator steps back, not in a frame: a man rising beside a low lens shoved it 0.6 m in one frame; and never
		// nearer than OwnNearest of the clearance, however quick he is)
		{
			Vec Want;
			if (FellAt < 0.0 && (Shot == Kind::Shoulder || Shot == Kind::Low))
			{
				Want = ClearOfOwn(Now.Eye, Battle) - Now.Eye;
				Want.Z = 0.0;
			}
			if (Now.bCut)
			{
				OwnPush = Want;
				PushSwing = Vec();
			}
			else
			{
				const double W = Set.PushRate;
				const Vec Off = OwnPush - Want;
				const Vec Mix = PushSwing + Off * W;
				const double Decay = std::exp(-W * RealDt);
				OwnPush = Want + (Off + Mix * RealDt) * Decay;
				PushSwing = (PushSwing - Mix * (W * RealDt)) * Decay;
			}
			Now.Eye = Now.Eye + OwnPush;
			if (FellAt < 0.0 && !bShoveCut && (Shot == Kind::Shoulder || Shot == Kind::Low)) Now.Eye = ClearOfOwn(Now.Eye, Battle, Set.OwnClear * Set.OwnNearest);
		}
		Now.Look = LookEased + LookWobble;
		// The lens's height over banks and corn: over the ground round it and ahead of it (KeepAboveAhead), and over
		// standing corn or tall grass just over the ears, never among the stalks (where a few of them filled the picture
		// like planks); it looks as it did, only from higher. The stalks a step or two in front count as well: the second
		// cut's fallen man was seen through tall grass a metre from a lens with none under it, each seed head the size of
		// a door. It rises on a stiff spring, as an operator stands up, and comes down over half a second: lifted in a
		// frame, it leapt a metre as a fallen man's look swung over tall corn, and fell 1.4 m in four frames coming off a
		// bank's top. Only the ground under it and the corn under it lift it at once.
		{
			const double Was = Now.Eye.Z;
			const double BankZ = (Shot == Kind::Open || Shot == Kind::Wide ? KeepAbove(Now.Eye, 0.9) : KeepAboveAhead(Now.Eye, WantEye, Shot == Kind::Low ? 0.25 : 0.9)).Z;
			const double Under = HeightAt(Now.Eye.X, Now.Eye.Y);
			double Hard = Under + 0.22, CornZ = -1e9, HardCorn = -1e9;
			if (Shot == Kind::Low || Shot == Kind::Shoulder || Shot == Kind::Tank)
			{
				const double Crop = CropAt(Now.Eye.X, Now.Eye.Y);
				if (Crop > 0.0) HardCorn = Under + Crop + OverEars;
				// (each over the ground where it stands, a bank's rise in front included)
				double Ears = -1e9;
				// (and the corn round it, a metre and more off: beside the edge of a field the hand's sway carried the lens
				// over the first stalks and back, and it leapt up half a metre)
				for (int K = 0; K < 8; ++K)
				{
					const Vec P = Now.Eye + Vec(std::cos(K * Pi / 4.0), std::sin(K * Pi / 4.0), 0.0) * 1.3;
					const double There = CropAt(P.X, P.Y);
					if (There > 0.0) Ears = std::max(Ears, HeightAt(P.X, P.Y) + There);
				}
				// (and where the lens is going: the corn it will be over in the next second, by where its man will be, so it
				// is up before it gets there: a lens running with men on the approach crosses 14 m of it, and needs half a
				// second to rise)
				Vec AtNow;
				double FacingNow = 0.0;
				if (Subject >= 0 && WhereIs(Subject, Battle, AtNow, FacingNow))
				{
					for (const double Later : { 0.25, 0.5, 0.75, 1.0, 1.4 })
					{
						Vec AtThen;
						double FacingThen = 0.0;
						if (!WhereIs(Subject, Battle + Soon(Later), AtThen, FacingThen)) break;
						const Vec P = Now.Eye + Vec(AtThen.X - AtNow.X, AtThen.Y - AtNow.Y, 0.0);
						const double There = CropAt(P.X, P.Y);
						if (There > 0.0) Ears = std::max(Ears, HeightAt(P.X, P.Y) + There);
					}
				}
				Vec Front = Now.Look - Now.Eye;
				Front.Z = 0.0;
				const double Flat = Front.Length();
				for (const double Step : AheadSteps)
				{
					if (Flat < 0.5 || Step > Flat - 0.4) break;
					const Vec P = Now.Eye + Front * (Step / Flat);
					const double There = CropAt(P.X, P.Y);
					if (There > 0.0) Ears = std::max(Ears, HeightAt(P.X, P.Y) + There);
				}
				// (and the corn farther on, met early up a slope (Settings::Climb), so the lens is mostly up before the
				// stalks a step in front)
				for (const double Step : { 3.2, 4.0, 4.8, 5.6 })
				{
					if (Flat < 0.5 || Step > Flat - 0.4) break;
					const Vec P = Now.Eye + Front * (Step / Flat);
					const double There = CropAt(P.X, P.Y);
					if (There > 0.0) Ears = std::max(Ears, HeightAt(P.X, P.Y) + There - Set.Climb * (Step - AheadSteps[3]));
				}
				if (Ears > -1e8) CornZ = Ears + OverEars;
				CornZ = std::max(CornZ, HardCorn);
			}
			// (the corn under it lifts it at once, and holds it, coming down over a second as it leaves the corn: patchy
			// corn under a running lens must not pop it up and down)
			{
				const double CornNow = HardCorn > -1e8 ? HardCorn : Under;
				if (Now.bCut || CornNow >= CornHeld) CornHeld = CornNow;
				else
				{
					const double W = 2.5;
					const double Off = CornHeld - CornNow;
					const double Mix = CornSwing + Off * W;
					const double Decay = std::exp(-W * RealDt);
					CornHeld = CornNow + (Off + Mix * RealDt) * Decay;
					CornSwing = (CornSwing - Mix * (W * RealDt)) * Decay;
				}
				if (Now.bCut || CornNow >= CornHeld) CornSwing = 0.0;
				Hard = std::max(Hard, CornHeld);
			}
			// (the corn foreseen is held a moment, let down at a metre a second: sampled along his path a patch of tall corn
			// came and went from frame to frame, and the lens never rose to it before it was over it)
			ForeHeld = Now.bCut ? CornZ : std::max(CornZ, ForeHeld - 1.0 * RealDt);
			const double LiftWant = std::max(0.0, std::max(BankZ, ForeHeld) - Was);
			if (Now.bCut)
			{
				Lift = LiftWant;
				LiftSwing = 0.0;
			}
			else
			{
				// (up as an operator stands, down more slowly; both on a spring, so neither starts with a jolt)
				const double W = LiftWant >= Lift ? Set.LiftRate : 4.4;
				const double Off = Lift - LiftWant;
				const double Mix = LiftSwing + Off * W;
				const double Decay = std::exp(-W * RealDt);
				Lift = LiftWant + (Off + Mix * RealDt) * Decay;
				LiftSwing = (LiftSwing - Mix * (W * RealDt)) * Decay;
			}
			Now.Eye.Z = std::max(Was + Lift, Hard);
			// (the look lifted with the corn's share, so the shot keeps its tilt; on a man fallen it comes down onto him
			// over a moment, not in a frame: a shoulder shot's look dropped 0.44 m in one frame as its man was hit)
			const double CornLift = std::max(0.0, std::min(Now.Eye.Z, std::max(CornZ, HardCorn)) - std::max(Was, BankZ));
			const double LookWant = FellAt < 0.0 ? CornLift : 0.0;
			if (Now.bCut)
			{
				LookLift = LookWant;
				LookLiftSwing = 0.0;
			}
			else
			{
				const double W = 6.0;
				const double Off = LookLift - LookWant;
				const double Mix = LookLiftSwing + Off * W;
				const double Decay = std::exp(-W * RealDt);
				LookLift = LookWant + (Off + Mix * RealDt) * Decay;
				LookLiftSwing = (LookLiftSwing - Mix * (W * RealDt)) * Decay;
			}
			Now.Look.Z += LookLift;
		}
		// (and after it, as it is lifted: lifted toward a man rising beside a low lens, the lens came within 0.7 m of
		// his head)
		if (FellAt < 0.0 && !bShoveCut && (Shot == Kind::Shoulder || Shot == Kind::Low)) Now.Eye = ClearOfOwn(Now.Eye, Battle, Set.OwnClear * Set.OwnNearest);
		Now.Roll = (Wave(2, 0.17) * 0.6 + Wave(4, 0.53) * 0.2) * Hand;
		// The shallow focus (E9): whether the shot has it is settled at the cut, so the scene never switches its depth of
		// field in the middle of a shot; within it the distance is pulled after the subject on a spring (in its log: a man
		// walking from 20 m to 10 m is pulled as far as one from 80 m to 40 m), not snapped to him.
		{
			const double Want = FocusWanted(Battle);
			if (Now.bCut)
			{
				bFocusShot = Want > 0.0 && Want <= Set.FocusFar;
				FocusEased = std::clamp(Want, 0.5, std::max(0.5, Set.FocusMost));
				FocusSwing = 0.0;
			}
			else if (bFocusShot && Want > 0.0)
			{
				const double W = Set.FocusPull;
				const double Off = std::log(FocusEased) - std::log(std::clamp(Want, 0.5, std::max(0.5, Set.FocusMost)));
				const double Mix = FocusSwing + Off * W;
				const double Decay = std::exp(-W * RealDt);
				FocusEased = std::clamp(Want, 0.5, std::max(0.5, Set.FocusMost)) * std::exp((Off + Mix * RealDt) * Decay);
				FocusSwing = (FocusSwing - Mix * (W * RealDt)) * Decay;
			}
			Now.Focus = bFocusShot ? FocusEased : 0.0;
		}
		Now.Shot = Shot;
		Now.Subject = Subject;
		// (and for what the scene asks between frames)
		++FrameStamp;
		return Now;
	}
}
