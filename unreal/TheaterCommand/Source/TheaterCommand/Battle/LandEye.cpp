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
		BadSince = -1.0;
		bClosed = false;
		LastFallCut = -100.0;
		bNearWatch = false;
		bFallEye = false;
		ShownAt.assign(Rec->Tracks.size(), -1e9);
		LastTankSide = -1;
		ShooterOf.clear();
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
			if (W == "mortar" || W == "gun" || W == "tank" || W == "at") Heavy.push_back(static_cast<int>(I));
		}
		std::sort(Heavy.begin(), Heavy.end(), [this](int A, int B) { return Rec->Rounds[static_cast<size_t>(A)].L < Rec->Rounds[static_cast<size_t>(B)].L; });
		ByLanding.clear();
		ByLandingRounds = 0;
		IndexLandings();
		BlockIx.clear();
		BlockIxTracks = BlockIxBlocks = 0;
		MenT = -1e18;
		MenTracks = 0;
		// (the fastest a man goes between two of his points, and on past his last at that speed)
		MenFastest = 0.0;
		for (const Track& Tr : Rec->Tracks)
		{
			if (Tr.bTwin || !Tr.IsMan()) continue;
			for (size_t K = 1; K < Tr.At.size(); ++K)
			{
				const double Dt = std::max(1e-6, Tr.At[K].T - Tr.At[K - 1].T);
				MenFastest = std::max(MenFastest, std::hypot(Tr.At[K].X - Tr.At[K - 1].X, Tr.At[K].Y - Tr.At[K - 1].Y) / Dt);
			}
		}
		// The film's own section: of the own side's rifle sections of four men or more, the one that fires most and
		// lasts longest in the fight (the recording is the script).
		HeroBlock = HeroK = -1;
		{
			const double Contact = Rec->Contact >= 0.0 ? Rec->Contact : 0.0;
			const double Finish = std::max(Rec->End, Contact + 1.0);
			std::map<std::pair<int, int>, std::pair<double, int>> Squads;
			for (const Track& Tr : Rec->Tracks)
			{
				if (Tr.bTwin || !Tr.IsMan()) continue;
				const LandFeed::Block* B = Rec->BlockOf(Tr.Block);
				if (!B || B->Side != Set.Own || !B->Team.empty()) continue;
				const double Until = Tr.Fell >= 0.0 ? Tr.Fell : (Tr.Out >= 0.0 ? Tr.Out : Finish);
				const double Lasted = std::clamp((Until - Contact) / (Finish - Contact), 0.0, 1.0);
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
		Nerves = NervesWant = 0.0;
		SinceSquad = 0;
		UsedSpots.clear();
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

	void Director::Ask(Kind K, int InSubject)
	{
		bAsked = true;
		AskedKind = K;
		AskedSubject = InSubject;
		bCutAsked = true;
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
			if (Tr.bTwin || !Tr.IsMan()) continue;
			const LandFeed::Spot S = LandFeed::SpotAt(Tr, T);
			const size_t B = BucketOf(static_cast<int64_t>(std::floor(S.X / MenSquare)), static_cast<int64_t>(std::floor(S.Y / MenSquare)), MenBuckets);
			if (MenHead[B] < 0) MenUsed.push_back(static_cast<uint32_t>(B));
			MenNext.push_back(MenHead[B]);
			MenHead[B] = static_cast<int32_t>(MenAt.size());
			MenAt.push_back({ static_cast<int>(I), S.X, S.Y });
		}
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
		for (size_t I = RoundFrom; I < Rec->Rounds.size(); ++I)
		{
			const LandFeed::Round& R = Rec->Rounds[I];
			if (R.T > To) break; // (rounds are in firing order; none fired after To lands before it)
			if (R.L < From || R.L > To) continue;
			Ahead.push_back({ R.X, R.Y, R.L, WeightOf(R.W) * (R.R == LandFeed::Result::Kill ? 3.0 : 1.0) });
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
		const double Steps[3] = { 0.45, Set.ManClear, Set.ThingClear };
		for (const double R : Steps)
		{
			for (int K = 0; K < 14; ++K)
			{
				const int What = Here.Solid(Eye + RoundWay(K) * R, Except);
				if (What == 2 && R <= Set.ThingClear + 1e-9) return false;
				if (What == 1 && R <= Set.ManClear + 1e-9) return false;
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

	bool Director::LensBurst(const Vec& Eye, const Vec& Look, double Fov, double From, double To, int Own, double* OutWhen) const
	{
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
			if (Gap >= Set.BurstClear * Set.BurstBig) continue;
			// (one striking the shot's own man or tank is its story: his fall, her knock-out; only right at the lens is
			// it cut)
			const bool bOwn = Own >= 0 && R.On >= 0 && TrackOfShooter(R.On, R.Ok, R.Oj) == Own;
			const double Clear = Set.BurstClear * (R.W == "gun" ? Set.BurstBig : 1.0);
			if (Gap < Set.BurstAround || (!bOwn && Gap < Clear && InPicture(P, At, 1.3)))
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

	bool Director::Crossing(const Vec* Eyes, const double* Times, int N, int Own) const
	{
		if (!Rec || N <= 0) return false;
		double Moved = 0.0;
		for (int K = 1; K < N; ++K) Moved = std::max(Moved, Flat(Eyes[K], Eyes[0]));
		const double From = Times[0], To = Times[N - 1];
		// (a man runs some 6 m a second at most, 12 on the rules' clock, twice life's: farther off than that, none can
		// reach the lens in time)
		const double Reach = Set.CrossClear + 12.0 * (To - From) + Moved + 0.5;
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
						if (std::hypot(S.X - Eyes[K].X, S.Y - Eyes[K].Y) >= Set.CrossClear) continue;
						// (his body from the ground to the top of him: standing, kneeling, lying or fallen)
						const bool bFallen = Tr.Fell >= 0.0 && Tr.Fell <= Times[K];
						const LandFeed::Change* C = LandFeed::ChangeAt(Tr, Times[K]);
						const std::string Pose = C ? C->Pose : std::string("idle");
						const double Top = bFallen || IsDown(Pose) ? 0.45 : (Pose == "fire_kneel" || Pose == "wait" ? 1.1 : 1.75);
						if (Eyes[K].Z < HeightAt(S.X, S.Y) + Top + 0.3) return true;
					}
				}
			}
		}
		return false;
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
		FellAt = -1.0;
		bFallEye = false;
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
		if (Part == 1) return !BadPicture(E, L, Now.Fov, bLoose ? Set.ScreenLeave : Set.ScreenMost);
		// (and a shot that follows its subject still a fit picture where the eye will be in a moment: not about to drive
		// into a hedge)
		if (Part >= 2)
		{
			Intended(Battle + Soon(Part == 2 ? 1.5 : 3.0), E, L);
			return !BadPicture(E, L, Now.Fov, Set.ScreenLeave);
		}
		// (a place on the ground among the men is used once in the film: the third cut ran the same over-the-shoulder
		// by a dark bush three times)
		if ((Shot == Kind::Shoulder || Shot == Kind::Low) && !bLoose)
			for (const Vec& Was : UsedSpots)
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
		// (no shell landing at the lens in its first seconds: the second cut went near black for 0.8 s)
		for (double U = 0.0; U < Set.ShotLeast + 0.5; U += 1.0)
		{
			const double T = Battle + Soon(U);
			Vec Ek = E, Lk = L;
			if (U > 0.0) Intended(T, Ek, Lk);
			if (LensBurst(Ek, Lk, Now.Fov, T - Soon(0.5), T + Soon(0.5), Subject, nullptr)) return false;
		}
		// (a watched man or a tracked tank in sight, not behind a tree or a hedge's shrubs)
		if ((Shot == Kind::Watch || Shot == Kind::Track) && !SeenFrom(E, L, Subject)) return false;
		// (its own man in the picture from its first frame, not down in the corn out of sight: a watch shot opened on a
		// man already lost to it, and held five seconds)
		if (!bLoose && Shot != Kind::Tank && Shot != Kind::Track && Subject >= 0 && Rec->Tracks[static_cast<size_t>(Subject)].IsMan())
		{
			Pose At;
			At.Eye = E;
			At.Look = L;
			At.Fov = Now.Fov;
			if (OwnLostFrom(At, Battle)) return false;
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
		K.Stand = Stand;
		K.Target = Target;
		K.TrackOff = TrackOff;
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
		Stand = K.Stand;
		Target = K.Target;
		TrackOff = K.TrackOff;
	}

	void Director::BeginChoice(double Battle, double Real)
	{
		const double Contact = Rec->Contact >= 0.0 ? Rec->Contact : 0.0;
		const bool bFighting = Battle >= Contact - 5.0;
		Options.clear();
		bScanning = false;
		bool bAskedNow = false;
		int Coming = Pending;
		Pending = -1;
		// (a tank about to be knocked out comes before a man's fall the last cut was going to; and one due before this
		// shot could be left for her, gone to now: the camera cut to a man 3.4 s before a tank's end, held its least
		// three seconds, and her window was gone by a frame)
		if ((Coming < 0 || !Rec->Tracks[static_cast<size_t>(Coming)].IsVehicle()) && !bAsked)
		{
			for (size_t I = 0; I < Rec->Tracks.size(); ++I)
			{
				const Track& Tr = Rec->Tracks[I];
				if (!Tr.IsVehicle() || Tr.bTwin || Tr.Fell < Battle + Soon(1.0) || Tr.Fell > Battle + Soon(Set.ShotLeast + 2.2)) continue;
				if (HiddenNow(static_cast<int>(I))) continue;
				if (std::find(Appointed.begin(), Appointed.end(), static_cast<int>(I)) != Appointed.end()) continue;
				Appointed.push_back(static_cast<int>(I));
				Coming = static_cast<int>(I);
				break;
			}
		}
		if (bAsked)
		{
			bAsked = false;
			bAskedNow = true;
			Options.push_back({ AskedKind, AskedSubject, false });
			Coming = -1;
		}
		else if (Battle > Rec->End + 1.0 && !bClosed)
		{
			// The closing aerial, once.
			Options.push_back({ Kind::Wide, -1, false });
			Coming = -1;
		}
		else
		{
			// The tanks' turn: no tank or tracking shot in the last two, so this one goes to a tank of either side (a
			// battle of hundreds of men would otherwise never leave them: Brian's moving tanks, a third of the shots).
			bool bTankTurn = bFighting && Recent.size() >= 2;
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
					if (Tr.bTwin || Tr.Fell < Battle + Soon(1.0) || Tr.Fell > Battle + Soon(3.0) || HiddenNow(static_cast<int>(I))) continue;
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
						if (static_cast<int>(I) == Coming || Tm.bTwin || !Tm.IsMan() || Tm.Start() > Battle || HiddenNow(static_cast<int>(I))) continue;
						if (Tm.Fell >= 0.0 && Tm.Fell < Battle + Soon(6.0)) continue;
						const LandFeed::Block* Bm = BlockOfTrack(static_cast<int>(I));
						if (!Bm || !Bf || Bm->Side != Bf->Side) continue;
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
				for (const Track& Tr : Rec->Tracks) bTanksOnly = bTanksOnly || (Tr.Role == "tank" && Standing(Tr));
			// (and in the film's own section's turn, every SquadEvery shots, only its men, while one still stands)
			bool bSquadOnly = false;
			if (!bTanksOnly && HeroBlock >= 0 && SinceSquad >= Set.SquadEvery - 1)
				for (size_t I = 0; I < Rec->Tracks.size() && !bSquadOnly; ++I) bSquadOnly = InSquad(static_cast<int>(I)) && Standing(Rec->Tracks[I]);
			// (the weighing itself goes on in ScanChoice, a frame's share at a time)
			bScanning = true;
			ChoiceScan = 0;
			for (Pick& P : ChoiceTop) P = Pick();
			bScanOwnTurn = bOwnTurn;
			bScanTanksOnly = bTanksOnly;
			bScanSquadOnly = bSquadOnly;
			bScanFallsDue = bFallsDue;
			bScanFighting = bFighting;
		}
		// (the turns asked of the men: about a thirty-second of a ray's step each, in the choice's count)
		RaySteps += static_cast<long>(Rec->Tracks.size() / 32);
		bChoiceAsked = bAskedNow;
		bChoiceMiddle = !bAskedNow && Battle > 12.0 && Battle < Rec->End;
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
			if (RaySteps - Start >= Budget) return false;
			const size_t I = ChoiceScan;
			const Track& Tr = Rec->Tracks[I];
			if (!Standing(Tr)) continue;
			if (Tr.Role == "lorry" || Tr.Role == "gun") continue;
			const LandFeed::Block* B = BlockOfTrack(static_cast<int>(I));
			if (!B) continue;
			const bool bTank = Tr.Role == "tank";
			if (bScanTanksOnly && !bTank) continue;
			if (bScanSquadOnly && !InSquad(static_cast<int>(I))) continue;
			if ((B->Side == Set.Own) != bScanOwnTurn && !bScanTanksOnly && !bScanSquadOnly) continue;
			if (HiddenNow(static_cast<int>(I))) continue;
			const LandFeed::Spot S = LandFeed::SpotAt(Tr, Battle);
			double Near = 0.0;
			long Seen = 0;
			const double Far = HeatNear(S.X, S.Y, Battle + Soon(0.5), Battle + Soon(8.0), 70.0, Battle + Soon(0.5), Battle + Soon(2.5), 40.0, Near, Seen);
			// (the heats looked at: about a twelfth of a ray's step each, and the man's own weighing two)
			RaySteps += 2 + Seen / 12;
			double Score = Near * 2.5 + Far;
			// Someone about to fall right here: the moment the film is for. Seen, not dwelt on.
			if (Tr.Fell >= Battle + Soon(1.0) && Tr.Fell < Battle + Soon(7.0)) Score += bScanFallsDue || Tr.IsVehicle() ? 12.0 : 3.0;
			// Moving men read best before the fight; men firing during it. Brian's moving tanks most of all.
			const LandFeed::Change* C = LandFeed::ChangeAt(Tr, Battle);
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
			if (TryPart > 0 && RaySteps - Start >= Budget)
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
		auto Spent = [&] { return RaySteps - Start >= Budget; };
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
				for (; ChoiceV < Variants && ChoiceTries < TriesACut; ++ChoiceV)
				{
					if (!bTrying && Spent()) return false;
					bNearWatch = Op.bNear;
					const int Fit = TryPlace(Op.K, Op.Who, Battle, ChoiceV, Start, Budget);
					if (Fit < 0) return false;
					if (Fit > 0) return true;
					++ChoiceTries;
				}
				if (bChoiceAsked && ChoiceO == 0)
				{
					bNearWatch = false;
					if (Place(Op.K, Op.Who, Battle, 0)) return true;
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
			if (bMiddle)
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
		Place(Kind::Wide, -1, Battle);
		return true;
	}

	void Director::FinishChoice(double Battle, double Real)
	{
		const int Coming = ChoiceComing;
		bChoosing = false;
		bLoose = false;
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
		BadSince = -1.0;
		BurstCutAt = -1.0;
		LostSince = -1.0;
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
		++CutCount;
		CutLog.emplace_back(Battle, Shot);
		Now.bCut = true;
	}

	bool Director::InSquad(int TrackIndex) const
	{
		if (!Rec || HeroBlock < 0 || TrackIndex < 0 || static_cast<size_t>(TrackIndex) >= Rec->Tracks.size()) return false;
		const Track& Tr = Rec->Tracks[static_cast<size_t>(TrackIndex)];
		return Tr.Block == HeroBlock && Tr.K == HeroK && Tr.IsMan();
	}

	bool Director::HiddenNow(int TrackIndex) const
	{
		if (!Rec || !Here.Hidden || TrackIndex < 0 || static_cast<size_t>(TrackIndex) >= Rec->Tracks.size()) return false;
		return Here.Hidden(Rec->Tracks[static_cast<size_t>(TrackIndex)].Block);
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

	Pose Director::Update(double Battle, double Real, double RealDt)
	{
		Now.bCut = false;
		if (!Rec) return Now;
		// (the frame's work so far, by the ground asked under the camera's rays: a cut's choice has what is left of the
		// frame's share)
		const long FrameFrom = RaySteps;
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
		if (!bChoosing && !bHeld && !bCut)
		{
			if (Shot == Kind::Open && Held > Set.Opening) { bCut = true; Reason = Why::Opening; }
			else if (Shot != Kind::Open && Held > ShotLength && FellAt < 0.0) { bCut = true; Reason = Why::Length; }
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
		if (!bChoosing && !bCut && !bHeld && Shot != Kind::Open && (FellAt < 0.0 || Real > FellAt + 0.8) && Held >= Set.ShotLeast)
		{
			for (size_t N = 0; N < 2 * Rec->Tracks.size() && !bCut; ++N)
			{
				const bool bVehicles = N < Rec->Tracks.size();
				const size_t I = bVehicles ? N : N - Rec->Tracks.size();
				const Track& Tr = Rec->Tracks[I];
				if (Tr.IsVehicle() != bVehicles || (!bVehicles && FellAt >= 0.0)) continue;
				if (Tr.bTwin || Tr.Fell < Battle + Soon(bVehicles ? 0.4 : 1.0) || Tr.Fell > Battle + Soon(2.2)) continue;
				if (HiddenNow(static_cast<int>(I))) continue;
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
		if (!bChoosing && !bCut && Here.Solid && Shot != Kind::Open && Shot != Kind::Wide && Real >= NextLook)
		{
			NextLook = Real + 0.25;
			bool bBad = !RoomAt(Now.Eye, Subject) || BlockedShare(Now.Eye, Now.Look, Now.Fov, Subject, Set.BlockedMost) > Set.BlockedMost ||
				ScreenShare(Now.Eye, Now.Look, Now.Fov, Subject, Set.ScreenLeave) > Set.ScreenLeave;
			// (where a following eye will be a moment on: left before it drives into a hedge, not once it is in it)
			if (!bBad && FellAt < 0.0 && (Shot == Kind::Track || Shot == Kind::Tank || Shot == Kind::Shoulder || Shot == Kind::Low))
			{
				Vec E, L;
				Intended(Battle + Soon(Set.AheadLook), E, L);
				bBad = BadPicture(E, L, Now.Fov, Set.ScreenLeave);
			}
			// (its own man filling the picture; a man of the recording about to walk into the lens, where it will be)
			if (!bBad && FellAt < 0.0 && (Shot == Kind::Shoulder || Shot == Kind::Low))
				bBad = OwnShare(Now.Eye, Now.Look, Now.Fov, Subject) > Set.OwnMost;
			if (!bBad && FellAt < 0.0)
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
		// Its own man come into the lens at once (a man lying down turned about where he lay, and his head was a hand from
		// the lens): no operator steps a metre in a frame; the film cuts away.
		if (!bChoosing && !bCut && !bHeld && FellAt < 0.0 && (Shot == Kind::Shoulder || Shot == Kind::Low) && OwnDistance(Now.Eye, Subject, Battle) < Set.OwnClear * Set.OwnCutInside)
		{
			bCut = true;
			Reason = Why::Rescue;
		}
		if (bCut || bChoosing)
		{
			// (the cut's choice: begun now, or going on from the frames before; this frame's share of its tries, the
			// shot on the screen kept to be put back if none is placed yet)
			if (!bChoosing)
			{
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
			WantEye = Stand + Across * (ShotHeld * 2.5);
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
			if (FellAt < 0.0 && (Shot == Kind::Shoulder || Shot == Kind::Low)) Now.Eye = ClearOfOwn(Now.Eye, Battle, Set.OwnClear * Set.OwnNearest);
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
		if (FellAt < 0.0 && (Shot == Kind::Shoulder || Shot == Kind::Low)) Now.Eye = ClearOfOwn(Now.Eye, Battle, Set.OwnClear * Set.OwnNearest);
		Now.Roll = (Wave(2, 0.17) * 0.6 + Wave(4, 0.53) * 0.2) * Hand;
		Now.Shot = Shot;
		Now.Subject = Subject;
		return Now;
	}
}
