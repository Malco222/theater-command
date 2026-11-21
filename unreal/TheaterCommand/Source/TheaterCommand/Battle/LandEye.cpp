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

		Vec Ahead(double Facing, double Far)
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
		}
		return "?";
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
		Place(Kind::Open, -1, 0.0);
		EyeEased = Stand;
		LookEased = Target;
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

	double Director::HeatAt(double X, double Y, double From, double To, double Radius) const
	{
		double Heat = 0.0;
		for (const LandFeed::Round& R : Rec->Rounds)
		{
			if (R.L < From) continue;
			if (R.T > To) break; // (rounds are in firing order; none fired after To lands before it here that matters)
			if (R.L > To) continue;
			const double D = std::hypot(R.X - X, R.Y - Y);
			if (D > Radius) continue;
			Heat += WeightOf(R.W) * (1.0 - D / Radius) * (R.R == LandFeed::Result::Kill ? 3.0 : 1.0);
		}
		for (const Track& Tr : Rec->Tracks)
		{
			if (Tr.Fell < From || Tr.Fell > To || Tr.bTwin) continue;
			const LandFeed::Spot S = LandFeed::SpotAt(Tr, Tr.Fell);
			const double D = std::hypot(S.X - X, S.Y - Y);
			if (D > Radius) continue;
			Heat += (Tr.IsVehicle() ? 25.0 : 10.0) * (1.0 - D / Radius);
		}
		return Heat;
	}

	bool Director::Place(Kind K, int TrackIndex, double Battle)
	{
		Shot = K;
		Subject = TrackIndex;
		FellAt = -1.0;
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
			const double Gap = std::hypot(Ex - Sx, Ey - Sy);
			const double BackM = K == Kind::Open ? 420.0 : 160.0 + Roll() * 140.0;
			const double SideM = (Roll() * 2.0 - 1.0) * (K == Kind::Open ? 120.0 : 220.0);
			const Vec Base = Vec(Sx, Sy, 0.0) + Offset(Toward, BackM, SideM);
			Stand = Vec(Base.X, Base.Y, HeightAt(Base.X, Base.Y) + (K == Kind::Open ? 170.0 : 35.0 + Roll() * 35.0));
			const Vec Mid = Vec(Sx + (Ex - Sx) * 0.6, Sy + (Ey - Sy) * 0.6, 0.0);
			Target = Vec(Mid.X, Mid.Y, HeightAt(Mid.X, Mid.Y) + 2.0);
			(void)Gap;
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
		const double SideSign = Roll() < 0.5 ? -1.0 : 1.0;
		switch (K)
		{
		case Kind::Shoulder:
			Back = 1.3 + Roll() * 0.9;
			Side = SideSign * (0.45 + Roll() * 0.4);
			Up = IsDown(Pose) ? 0.55 : (Pose == "fire_kneel" || Pose == "wait" ? 1.15 : 1.55 + Roll() * 0.12);
			Now.Fov = 50.0 + Roll() * 12.0;
			break;
		case Kind::Low:
			Back = 0.9 + Roll() * 0.8;
			Side = SideSign * (0.7 + Roll() * 0.6);
			Up = 0.32 + Roll() * 0.15;
			Now.Fov = 46.0 + Roll() * 10.0;
			break;
		case Kind::Tank:
			Back = -1.0 + Roll() * 5.0;
			Side = SideSign * (3.0 + Roll() * 1.5);
			Up = 1.5 + Roll() * 0.6;
			Now.Fov = 52.0 + Roll() * 10.0;
			break;
		case Kind::Watch:
		{
			// From cover off to his own side's rear quarter, a long lens on him and the men round him.
			const double Away = 70.0 + Roll() * 70.0;
			const double Angle = Facing + Pi + SideSign * (0.5 + Roll() * 0.6);
			const Vec Base = At + Vec(std::sin(Angle) * Away, std::cos(Angle) * Away, 0.0);
			Stand = Vec(Base.X, Base.Y, HeightAt(Base.X, Base.Y) + 6.0 + Roll() * 6.0);
			Target = At + Vec(0.0, 0.0, 1.0);
			Now.Fov = 20.0 + Roll() * 10.0;
			// A line of sight blocked (a hedge, a house): try the other quarter, then nearer.
			if (Here.Open && !Here.Open(Stand, Target))
			{
				for (int Try = 0; Try < 4; ++Try)
				{
					const double A2 = Facing + Pi + (Try % 2 == 0 ? -SideSign : SideSign) * (0.4 + 0.3 * Try);
					const double Away2 = Away * (1.0 - 0.18 * Try);
					const Vec B2 = At + Vec(std::sin(A2) * Away2, std::cos(A2) * Away2, 0.0);
					const Vec S2 = Vec(B2.X, B2.Y, HeightAt(B2.X, B2.Y) + 8.0 + 2.0 * Try);
					if (Here.Open(S2, Target)) { Stand = S2; break; }
				}
			}
			return true;
		}
		default:
			break;
		}
		return true;
	}

	void Director::Choose(double Battle, double Real)
	{
		const double Contact = Rec->Contact >= 0.0 ? Rec->Contact : 0.0;
		const bool bFighting = Battle >= Contact - 5.0;
		Kind Want = Kind::Shoulder;
		int Best = -1;
		const bool bAskedNow = bAsked;
		if (bAsked)
		{
			bAsked = false;
			Want = AskedKind;
			Best = AskedSubject;
		}
		// The big moments, a few seconds ahead: a tank knocked out, a man falling. The camera is there before, as a
		// film's is (the recording is the script); most men's falls, every tank's.
		bool bAppointment = false;
		if (Best < 0 && !bHeld)
		{
			for (size_t I = 0; I < Rec->Tracks.size(); ++I)
			{
				const Track& Tr = Rec->Tracks[I];
				if (Tr.bTwin || Tr.Fell < Battle + 2.5 || Tr.Fell > Battle + 7.5) continue;
				if (std::find(Appointed.begin(), Appointed.end(), static_cast<int>(I)) != Appointed.end()) continue;
				Appointed.push_back(static_cast<int>(I));
				if (!Tr.IsVehicle() && Roll() > 0.75) continue;
				Best = static_cast<int>(I);
				if (Tr.IsVehicle()) Want = Kind::Watch;
				else
				{
					const LandFeed::Change* C = LandFeed::ChangeAt(Tr, Battle);
					const bool bDown = C && IsDown(C->Pose);
					const double Pick = Roll();
					Want = Pick < 0.45 ? (bDown ? Kind::Low : Kind::Shoulder) : Kind::Watch;
				}
				bAppointment = true;
				break;
			}
		}
		if (Best < 0 && Want != Kind::Wide && Want != Kind::Open)
		{
			// The candidates: men and tanks still up, scored by what is about to happen round them.
			double BestScore = -1.0;
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
				double Score = HeatAt(S.X, S.Y, Battle + 0.5, Battle + 8.0, 70.0);
				// Someone about to fall right here: the moment the film is for. Seen, not dwelt on.
				if (Tr.Fell >= Battle + 2.0 && Tr.Fell < Battle + 7.0) Score += 12.0;
				// Moving men read best before the fight; men firing during it.
				const LandFeed::Change* C = LandFeed::ChangeAt(Tr, Battle);
				if (!bFighting && S.V > 0.3) Score += 3.0;
				if (C && C->bAim) Score += 2.0;
				if (Tr.Role == "tank") Score += bFighting ? 0.5 : 2.0;
				if (static_cast<int>(I) == Subject) Score *= 0.3; // not the same man twice running
				Score += Roll() * 2.5;
				if (Score > BestScore) { BestScore = Score; Best = static_cast<int>(I); }
			}
			if (Best >= 0)
			{
				const Track& Tr = Rec->Tracks[static_cast<size_t>(Best)];
				const LandFeed::Change* C = LandFeed::ChangeAt(Tr, Battle);
				const std::string Pose = C ? C->Pose : std::string("idle");
				const double Pick = Roll();
				if (Tr.Role == "tank") Want = Pick < 0.5 ? Kind::Tank : Kind::Watch;
				else if (IsDown(Pose)) Want = Pick < 0.55 ? Kind::Low : (Pick < 0.8 ? Kind::Shoulder : Kind::Watch);
				else Want = Pick < 0.7 ? Kind::Shoulder : Kind::Watch;
				// A wide shot now and then, to say where everyone is: never twice in four.
				const bool bWideLately = std::find(Recent.begin(), Recent.end(), Kind::Wide) != Recent.end();
				if (!bWideLately && Roll() < (bFighting ? 0.12 : 0.25)) { Want = Kind::Wide; Best = -1; }
				// Never the same kind three times running.
				if (Recent.size() >= 2 && Recent[Recent.size() - 1] == Want && Recent[Recent.size() - 2] == Want && Want != Kind::Wide)
					Want = Want == Kind::Watch ? Kind::Shoulder : Kind::Watch;
			}
			else Want = Kind::Wide;
		}
		if (Battle > Rec->End + 1.0) { Want = Kind::Wide; Best = -1; }
		// A man down in standing corn or tall grass: the lens over his shoulder, not low among the stalks (a shot asked
		// for keeps its kind, and is lifted over the ears in Update).
		if (Want == Kind::Low && Best >= 0 && !bAskedNow)
		{
			Vec At;
			double Facing = 0.0;
			if (WhereIs(Best, Battle, At, Facing) && CropAt(At.X, At.Y) > LowInCrop) Want = Kind::Shoulder;
		}
		if (!Place(Want, Best, Battle)) Place(Kind::Wide, -1, Battle);
		Recent.push_back(Shot);
		if (Recent.size() > 4) Recent.erase(Recent.begin());
		ShotStarted = Real;
		BattleAtCut = Battle;
		ShotLength = Set.ShotMin + Roll() * (Set.ShotMax - Set.ShotMin);
		if (Shot == Kind::Wide) ShotLength *= 0.7;
		// (an appointment holds to the moment: the fall's own hold then cuts three seconds after it)
		if (bAppointment && Subject >= 0) ShotLength = std::max(ShotLength, Rec->Tracks[static_cast<size_t>(Subject)].Fell - Battle + 3.0);
		++CutCount;
		CutLog.emplace_back(Battle, Shot);
		Now.bCut = true;
	}

	Pose Director::Update(double Battle, double Real, double RealDt)
	{
		Now.bCut = false;
		if (!Rec) return Now;
		const double Held = Real - ShotStarted;
		bool bCut = bCutAsked;
		bCutAsked = false;
		if (!bHeld)
		{
			if (Shot == Kind::Open && Held > Set.Opening) bCut = true;
			else if (Shot != Kind::Open && Held > ShotLength && FellAt < 0.0) bCut = true;
		}
		// His fall: hold on him three battle seconds, then cut (bodies stay; the film moves on).
		if (Subject >= 0)
		{
			const Track& Tr = Rec->Tracks[static_cast<size_t>(Subject)];
			if (Tr.Fell >= 0.0 && Tr.Fell <= Battle && FellAt < 0.0) FellAt = Battle;
			if (FellAt >= 0.0 && Battle > FellAt + 3.0 && !bHeld) bCut = true;
			if (Tr.Out >= 0.0 && Battle > Tr.Out) bCut = true;
		}
		if (bCut) Choose(Battle, Real);

		Vec WantEye = Stand, WantLook = Target;
		if (Shot == Kind::Open)
		{
			// Coming down over the attack toward the field: 170 m to 70 m over the opening.
			const double U = std::clamp(Held / std::max(1.0, Set.Opening), 0.0, 1.0);
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
			WantEye = Stand + Across * (Held * 2.5);
		}
		else if (Subject >= 0)
		{
			Vec At;
			double Facing = 0.0;
			WhereIs(Subject, Battle, At, Facing);
			// The operator turns with him, not at once.
			FacingEased = LandFeed::TurnToward(FacingEased, Facing, 1.0 - std::exp(-RealDt / 1.1));
			if (Shot == Kind::Watch)
			{
				WantLook = At + Vec(0.0, 0.0, 1.0);
			}
			else
			{
				WantEye = At + Offset(FacingEased, Back, Side) + Vec(0.0, 0.0, Up);
				const double LookFar = Shot == Kind::Tank ? 70.0 : 35.0;
				WantLook = At + Ahead(FacingEased, LookFar);
				WantLook.Z = HeightAt(WantLook.X, WantLook.Y) + (Shot == Kind::Low ? 0.6 : 1.2);
				// (a subject fallen: the eye drops to him and looks down at him a moment)
				if (FellAt >= 0.0)
				{
					WantLook = At + Vec(0.0, 0.0, 0.2);
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
			EyeEased = Ease(EyeEased, WantEye, bFollow ? 0.35 : 0.8, RealDt);
			LookEased = Ease(LookEased, WantLook, bFollow ? 0.25 : 0.6, RealDt);
		}
		// The hand-held wobble: a few slow sines, stronger on the ground among the men, slight from a crane.
		const double Hand = Set.Hand * (Shot == Kind::Shoulder || Shot == Kind::Low ? 1.0 : Shot == Kind::Tank ? 0.8 : Shot == Kind::Watch ? 0.35 : 0.15);
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
