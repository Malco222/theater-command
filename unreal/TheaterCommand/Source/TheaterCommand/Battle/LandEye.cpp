#include "Battle/LandEye.h"

#include <algorithm>
#include <cmath>

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
		if (Here.Height) return Here.Height(X, Y);
		return Rec ? Rec->Ground.HeightAt(X, Y) : 0.0;
	}

	bool Director::WhereIs(int TrackIndex, double T, Vec& OutAt, double& OutFacing) const
	{
		if (TrackIndex < 0 || !Rec) return false;
		if (Here.Where && Here.Where(TrackIndex, T, OutAt, OutFacing)) return true;
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
		OutAt.Z += Tr.IsVehicle() ? 1.3 : (bFallen ? 0.2 : 1.0);
		return true;
	}

	int Director::TrackOfShooter(int Block, int K) const
	{
		const int64_t Key = static_cast<int64_t>(Block) * 65536 + K;
		const auto It = std::lower_bound(ShooterOf.begin(), ShooterOf.end(), std::make_pair(Key, -1));
		return It != ShooterOf.end() && It->first == Key ? It->second : -1;
	}

	Vec Director::KeepAbove(Vec At, double Least) const
	{
		// The ground under the eye and a little round it (a slope rising just ahead of the lens).
		double Ground = HeightAt(At.X, At.Y);
		for (int K = 0; K < 4; ++K)
		{
			const double A = K * Pi * 0.5;
			Ground = std::max(Ground, HeightAt(At.X + std::cos(A) * 0.6, At.Y + std::sin(A) * 0.6) - 0.05);
		}
		At.Z = std::max(At.Z, Ground + Least);
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
		CropHold = 0.0;
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
		ShooterOf.clear();
		for (size_t I = 0; I < Rec->Tracks.size(); ++I)
		{
			const Track& Tr = Rec->Tracks[I];
			if (!Tr.bTwin) ShooterOf.emplace_back(static_cast<int64_t>(Tr.Block) * 65536 + Tr.K, static_cast<int>(I));
		}
		std::sort(ShooterOf.begin(), ShooterOf.end());
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
	}

	double Director::HeatNear(double X, double Y, double From, double To, double Radius) const
	{
		double Sum = 0.0;
		for (const Heat& H : Ahead)
		{
			if (H.T < From || H.T > To) continue;
			const double D = std::hypot(H.X - X, H.Y - Y);
			if (D > Radius) continue;
			Sum += H.W * (1.0 - D / Radius);
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

	bool Director::ActionIn(double From, double To) const
	{
		if (!(To > From)) return false;
		for (size_t I = RoundFrom; I < Rec->Rounds.size(); ++I)
		{
			const LandFeed::Round& R = Rec->Rounds[I];
			if (R.T > To) break;
			const bool bFired = R.T > From && R.T <= To;
			const bool bLanded = R.L > From && R.L <= To;
			if (bLanded)
			{
				const Vec At(R.X, R.Y, HeightAt(R.X, R.Y) + 0.5);
				if ((At - Now.Eye).Length() < 300.0 && InPicture(Now, At)) return true;
			}
			if (bFired)
			{
				const int Who = TrackOfShooter(R.By, R.K);
				Vec At;
				double Facing = 0.0;
				if (Who >= 0 && WhereIs(Who, To, At, Facing))
				{
					At.Z += 1.0;
					if ((At - Now.Eye).Length() < 250.0 && InPicture(Now, At)) return true;
				}
			}
		}
		for (const Track& Tr : Rec->Tracks)
		{
			if (Tr.bTwin || !(Tr.Fell > From && Tr.Fell <= To)) continue;
			const LandFeed::Spot S = LandFeed::SpotAt(Tr, Tr.Fell);
			const Vec At(S.X, S.Y, HeightAt(S.X, S.Y) + 0.8);
			if ((At - Now.Eye).Length() < 300.0 && InPicture(Now, At)) return true;
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
				const LandFeed::Block* B = Rec->BlockOf(Tr.Block);
				if (!B || (B->Kind == "art" && B->Team.empty())) continue; // the batteries stand far back
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
			Up = IsDown(Pose) ? 0.55 : (Pose == "fire_kneel" || Pose == "wait" ? 1.15 : 1.55 + Roll() * 0.12);
			Now.Fov = 50.0 + Roll() * 12.0;
			break;
		case Kind::Low:
			Back = 0.9 + Roll() * 0.8 + 0.4 * Further;
			Side = SideSign * (0.7 + Roll() * 0.6);
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
			OutEye = At + Offset(FacingEased, Back, Side) + Vec(0.0, 0.0, Up);
			OutLook = At + Before(FacingEased, Shot == Kind::Tank ? 70.0 : 35.0);
			OutLook.Z = HeightAt(OutLook.X, OutLook.Y) + (Shot == Kind::Low ? 0.6 : 1.2);
			const double Crop = CropAt(OutEye.X, OutEye.Y);
			if (Crop > 0.0) OutEye.Z = std::max(OutEye.Z, HeightAt(OutEye.X, OutEye.Y) + Crop + OverEars);
		}
		OutEye = KeepAbove(OutEye, Shot == Kind::Low ? 0.25 : 0.9);
	}

	bool Director::Fits(double Battle) const
	{
		if (Shot == Kind::Open || Shot == Kind::Wide) return true;
		Vec E, L;
		Intended(Battle, E, L);
		if (Here.Free && !Here.Free(E)) return false;
		if (!RoomAt(E, Subject)) return false;
		return BlockedShare(E, L, Now.Fov, Subject, Set.BlockedMost) <= Set.BlockedMost;
	}

	void Director::Choose(double Battle, double Real)
	{
		const double Contact = Rec->Contact >= 0.0 ? Rec->Contact : 0.0;
		const bool bFighting = Battle >= Contact - 5.0;
		struct Option
		{
			Kind K = Kind::Wide;
			int Who = -1;
			bool bNear = false;
		};
		std::vector<Option> Options;
		bool bAskedNow = false;
		int Coming = Pending;
		Pending = -1;
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
			// The big moments a second or two ahead: a tank knocked out, a man falling. The camera is there just before,
			// as a film's is (the recording is the script): every tank's, some men's, never one on another.
			if (Coming < 0 && !bHeld)
			{
				for (size_t I = 0; I < Rec->Tracks.size(); ++I)
				{
					const Track& Tr = Rec->Tracks[I];
					if (Tr.bTwin || Tr.Fell < Battle + 1.0 || Tr.Fell > Battle + 3.0) continue;
					if (std::find(Appointed.begin(), Appointed.end(), static_cast<int>(I)) != Appointed.end()) continue;
					Appointed.push_back(static_cast<int>(I));
					if (!Tr.IsVehicle() && (Real - LastFallCut < 6.0 || Roll() > 0.75)) continue;
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
					const LandFeed::Block* Bf = Rec->BlockOf(Tr.Block);
					int Mate = -1;
					double MateGap = 1e9;
					for (size_t I = 0; I < Rec->Tracks.size(); ++I)
					{
						const Track& Tm = Rec->Tracks[I];
						if (static_cast<int>(I) == Coming || Tm.bTwin || !Tm.IsMan() || Tm.Start() > Battle) continue;
						if (Tm.Fell >= 0.0 && Tm.Fell < Battle + 6.0) continue;
						const LandFeed::Block* Bm = Rec->BlockOf(Tm.Block);
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
			Gather(Battle + 0.5, Battle + 8.0);
			struct Pick
			{
				double Score = -1.0;
				int Who = -1;
			};
			Pick Top[4];
			const bool bOwnTurn = Roll() < Set.OwnShare;
			for (size_t I = 0; I < Rec->Tracks.size(); ++I)
			{
				const Track& Tr = Rec->Tracks[I];
				if (Tr.bTwin || (Tr.Fell >= 0.0 && Tr.Fell < Battle + 0.5) || Tr.Start() > Battle) continue;
				if (Tr.Out >= 0.0 && Tr.Out < Battle + 4.0) continue;
				if (Tr.Role == "lorry" || Tr.Role == "gun") continue;
				const LandFeed::Block* B = Rec->BlockOf(Tr.Block);
				if (!B) continue;
				if ((B->Side == Set.Own) != bOwnTurn) continue;
				const LandFeed::Spot S = LandFeed::SpotAt(Tr, Battle);
				double Score = HeatNear(S.X, S.Y, Battle + 0.5, Battle + 2.5, 40.0) * 2.5 + HeatNear(S.X, S.Y, Battle + 0.5, Battle + 8.0, 70.0);
				// Someone about to fall right here: the moment the film is for. Seen, not dwelt on.
				if (Tr.Fell >= Battle + 1.0 && Tr.Fell < Battle + 7.0) Score += 12.0;
				// Moving men read best before the fight; men firing during it. Brian's moving tanks most of all.
				const LandFeed::Change* C = LandFeed::ChangeAt(Tr, Battle);
				if (!bFighting && S.V > 0.3) Score += 3.0;
				if (C && C->bAim) Score += 2.0;
				if (Tr.Role == "tank")
				{
					Score += (bFighting ? 1.5 : 2.5) + (S.V > 0.5 ? 1.0 : 0.0);
					// (the tanks get a shot in turn with the men: never two running on tanks unless one is about to die)
					if (!Recent.empty() && (Recent.back() == Kind::Tank || Recent.back() == Kind::Track)) Score *= 0.4;
				}
				if (static_cast<int>(I) == Subject) Score *= 0.3; // not the same man twice running
				Score += Roll() * 2.5;
				for (int K = 0; K < 4; ++K)
				{
					if (Score <= Top[K].Score) continue;
					for (int J = 3; J > K; --J) Top[J] = Top[J - 1];
					Top[K] = { Score, static_cast<int>(I) };
					break;
				}
			}
			for (const Pick& P : Top)
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
					Options.push_back({ K, P.Who, false });
				}
			}
		}
		// Each option tried in turn, a few places each (the other side, farther, higher), until one is a fit picture:
		// the lens clear of things and men, the picture open. An asked shot keeps its kind even if none fits (the
		// picture run asks for that kind); with nothing fit, the high view (it always is).
		bool bPlaced = false;
		int Tries = 0;
		for (size_t O = 0; O < Options.size() && !bPlaced && Tries < TriesACut; ++O)
		{
			const Option& Op = Options[O];
			const int Variants = (Op.K == Kind::Wide || Op.K == Kind::Open) ? 1 : 4;
			for (int V = 0; V < Variants && Tries < TriesACut; ++V)
			{
				++Tries;
				bNearWatch = Op.bNear;
				if (Place(Op.K, Op.Who, Battle, V) && Fits(Battle))
				{
					bPlaced = true;
					break;
				}
			}
			if (!bPlaced && bAskedNow && O == 0)
			{
				bNearWatch = false;
				if (Place(Op.K, Op.Who, Battle, 0)) bPlaced = true;
			}
		}
		if (!bPlaced)
		{
			bNearWatch = false;
			Place(Kind::Wide, -1, Battle);
		}
		if (Shot == Kind::Wide && Battle > Rec->End) bClosed = true;
		Recent.push_back(Shot);
		if (Recent.size() > 4) Recent.erase(Recent.begin());
		ShotStarted = Real;
		BattleAtCut = Battle;
		NextLook = Real + 0.25;
		BadSince = -1.0;
		ShotLength = Set.ShotMin + Roll() * (Set.ShotMax - Set.ShotMin);
		if (Shot == Kind::Wide) ShotLength *= 0.7;
		// (a fall gone to holds for it: on him, his own fall's hold then cuts; on a man beside him, until it is done)
		if (Coming >= 0 && Shot != Kind::Wide)
		{
			const Track& Tc = Rec->Tracks[static_cast<size_t>(Coming)];
			ShotLength = std::max(ShotLength, Tc.Fell - Battle + Set.FallHold);
			if (!Tc.IsVehicle()) LastFallCut = Real;
		}
		++CutCount;
		CutLog.emplace_back(Battle, Shot);
		Now.bCut = true;
	}

	Pose Director::Update(double Battle, double Real, double RealDt)
	{
		Now.bCut = false;
		if (!Rec) return Now;
		// (the rounds that may still be in the air: none fired more than a minute before; a seek back starts again)
		if (Battle < LastBattle) RoundFrom = 0;
		while (RoundFrom < Rec->Rounds.size() && Rec->Rounds[RoundFrom].T < Battle - 60.0) ++RoundFrom;
		if (LastBattle >= 0.0 && Battle > LastBattle && ActionIn(LastBattle, Battle)) LastAction = Real;
		LastBattle = Battle;

		const double Held = Real - ShotStarted;
		const double Contact = Rec->Contact >= 0.0 ? Rec->Contact : 0.0;
		const bool bFighting = Battle >= Contact - 5.0;
		bool bCut = bCutAsked;
		Why Reason = Why::Asked;
		bCutAsked = false;
		if (!bHeld && !bCut)
		{
			if (Shot == Kind::Open && Held > Set.Opening) { bCut = true; Reason = Why::Opening; }
			else if (Shot != Kind::Open && Held > ShotLength && FellAt < 0.0) { bCut = true; Reason = Why::Length; }
			// Nothing has happened in it for a while, now that the armies are near: leave it.
			else if (Shot != Kind::Open && Shot != Kind::Wide && bFighting && FellAt < 0.0 && Held >= Set.ShotLeast &&
				Real - std::max(LastAction, ShotStarted) >= Set.DeadAfter)
			{
				bCut = true;
				Reason = Why::Dead;
			}
		}
		// His fall: kept in the picture to the ground and held where he lies, then cut (bodies stay; the film moves on).
		if (Subject >= 0)
		{
			const Track& Tr = Rec->Tracks[static_cast<size_t>(Subject)];
			if (Tr.Fell >= 0.0 && Tr.Fell <= Battle && FellAt < 0.0)
			{
				FellAt = Battle;
				FallEye = EyeEased;
				bFallEye = true;
			}
			if (!bCut && FellAt >= 0.0 && Battle > FellAt + Set.FallHold && !bHeld) { bCut = true; Reason = Why::Fall; }
			if (!bCut && Tr.Out >= 0.0 && Battle > Tr.Out) { bCut = true; Reason = Why::Gone; }
		}
		// Cut on action: a tank about to be knocked out, or a man about to fall, a second or two ahead and not in this
		// picture: go to it now, just before it happens. One already in the picture: hold for it.
		if (!bCut && !bHeld && Shot != Kind::Open && FellAt < 0.0 && Held >= Set.ShotLeast)
		{
			for (size_t I = 0; I < Rec->Tracks.size(); ++I)
			{
				const Track& Tr = Rec->Tracks[I];
				if (Tr.bTwin || Tr.Fell < Battle + 1.0 || Tr.Fell > Battle + 2.2) continue;
				if (std::find(Appointed.begin(), Appointed.end(), static_cast<int>(I)) != Appointed.end()) continue;
				Appointed.push_back(static_cast<int>(I));
				if (!Tr.IsVehicle() && (Real - LastFallCut < 6.0 || Roll() > 0.75)) continue;
				const LandFeed::Spot S = LandFeed::SpotAt(Tr, Battle);
				const Vec At(S.X, S.Y, HeightAt(S.X, S.Y) + 1.0);
				if (InPicture(Now, At, 0.9) && (At - Now.Eye).Length() < (Now.Fov < 30.0 ? 250.0 : 120.0))
				{
					ShotLength = std::max(ShotLength, Held + (Tr.Fell - Battle) + Set.FallHold);
					continue;
				}
				Pending = static_cast<int>(I);
				bCut = true;
				Reason = Why::Action;
				break;
			}
		}
		// Something come in front of the lens (a tank turning its gun across it, a man walking into it, the eye drifted
		// into a hedge): seen twice running, a quarter of a second apart, the shot is left at once.
		if (!bCut && Here.Solid && Shot != Kind::Open && Shot != Kind::Wide && Real >= NextLook)
		{
			NextLook = Real + 0.25;
			const bool bBad = !RoomAt(Now.Eye, Subject) || BlockedShare(Now.Eye, Now.Look, Now.Fov, Subject, Set.BlockedMost) > Set.BlockedMost;
			if (!bBad) BadSince = -1.0;
			else if (BadSince < 0.0) BadSince = Real;
			else if (!bHeld)
			{
				bCut = true;
				Reason = Why::Rescue;
			}
		}
		if (bCut)
		{
			LeftWhy = Reason;
			Choose(Battle, Real);
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
			// The operator turns with him, not at once.
			FacingEased = LandFeed::TurnToward(FacingEased, Facing, 1.0 - std::exp(-RealDt / 1.1));
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
				WantEye = At + Offset(FacingEased, Back, Side) + Vec(0.0, 0.0, Up);
				const double LookFar = Shot == Kind::Tank ? 70.0 : 35.0;
				WantLook = At + Before(FacingEased, LookFar);
				WantLook.Z = HeightAt(WantLook.X, WantLook.Y) + (Shot == Kind::Low ? 0.6 : 1.2);
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
		}
		else
		{
			const bool bFollow = Shot == Kind::Shoulder || Shot == Kind::Low || Shot == Kind::Tank;
			const double EyeEase = bFollow ? 0.35 : Shot == Kind::Track ? 0.5 : 0.8;
			const double LookEase = FellAt >= 0.0 ? 0.12 : (bFollow || Shot == Kind::Track) ? 0.25 : 0.6;
			EyeEased = Ease(EyeEased, WantEye, EyeEase, RealDt);
			LookEased = Ease(LookEased, WantLook, LookEase, RealDt);
		}
		// The hand-held wobble: a few slow sines, stronger on the ground among the men, slight from a crane.
		const double Hand = Set.Hand * (Shot == Kind::Shoulder || Shot == Kind::Low ? 1.0 : Shot == Kind::Tank ? 0.8 : Shot == Kind::Watch || Shot == Kind::Track ? 0.4 : 0.15);
		auto Wave = [&](int K, double Hz) { return std::sin(Real * Hz * 2.0 * Pi + Phase[K]); };
		const Vec Wobble = Vec(Wave(0, 0.37) + 0.5 * Wave(1, 1.13), Wave(2, 0.29) + 0.5 * Wave(3, 0.97), Wave(4, 0.41) + 0.4 * Wave(5, 1.31)) * (0.035 * Hand);
		const double LookFar = (LookEased - EyeEased).Length();
		const Vec LookWobble = Vec(Wave(1, 0.23), Wave(3, 0.31), Wave(5, 0.19)) * (LookFar * 0.006 * Hand);
		Now.Eye = KeepAbove(EyeEased + Wobble, Shot == Kind::Low ? 0.25 : 0.9);
		Now.Look = LookEased + LookWobble;
		// Over standing corn or tall grass a lens near the ground is held just over the ears, never among the stalks
		// (where a few of them filled the picture like planks); it looks as it did, only from higher. Up at once as the
		// corn under it rises, down over a second as it leaves it.
		{
			double Want = 0.0;
			if (Shot == Kind::Low || Shot == Kind::Shoulder || Shot == Kind::Tank)
			{
				const double Crop = CropAt(Now.Eye.X, Now.Eye.Y);
				if (Crop > 0.0) Want = Crop + OverEars;
			}
			if (Now.bCut || Want >= CropHold) CropHold = Want;
			else CropHold = Want + (CropHold - Want) * std::exp(-RealDt / 0.8);
			const double Floor = HeightAt(Now.Eye.X, Now.Eye.Y) + CropHold;
			if (CropHold > 0.0 && Now.Eye.Z < Floor)
			{
				const double Lifted = Floor - Now.Eye.Z;
				Now.Eye.Z = Floor;
				// (the look lifted with it, so the shot keeps its tilt; on a man fallen it stays on him)
				if (FellAt < 0.0) Now.Look.Z += Lifted;
			}
		}
		Now.Roll = (Wave(2, 0.17) * 0.6 + Wave(4, 0.53) * 0.2) * Hand;
		Now.Shot = Shot;
		Now.Subject = Subject;
		return Now;
	}
}
