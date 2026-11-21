#include "Battle/LandPlay.h"

#include <algorithm>
#include <cmath>

namespace LandPlay
{
	using LandFeed::Recording;
	using LandFeed::Result;
	using LandFeed::Round;
	using LandFeed::Track;

	double Vec::Length() const
	{
		return std::sqrt(X * X + Y * Y + Z * Z);
	}

	Vec Vec::Unit() const
	{
		const double L = Length();
		return L > 1e-9 ? Vec(X / L, Y / L, Z / L) : Vec(1.0, 0.0, 0.0);
	}

	namespace
	{
		bool IsOneOf(const std::string& S, std::initializer_list<const char*> List)
		{
			for (const char* L : List) if (S == L) return true;
			return false;
		}

		/** How high a man's muzzle is in a pose, metres over the ground (the soldier file's rifle at its aim: standing
		 * about 1.45, kneeling 1.05, lying 0.3). Only where the soldiers' core cannot say. */
		double MuzzleHeight(const std::string& Pose)
		{
			if (IsOneOf(Pose, { "fire_prone", "hide", "observe", "crawl", "down", "lmg_burst" })) return 0.3;
			if (IsOneOf(Pose, { "fire_kneel", "wait", "kneel_down", "cower" })) return 1.05;
			return 1.45;
		}

		/** The blast's size row for the sound, by the field's calibre (impacts.bySize). */
		std::string LandingSizeOf(const std::string& Cal)
		{
			if (Cal == "mortar") return "mortar";
			if (Cal == "howitzer") return "howitzer_105";
			if (Cal == "heavy") return "howitzer_150";
			if (Cal == "light") return "field_gun";
			return "field_gun";
		}

		/** A gun's calibre in millimetres for a glance's size: by the tank (75 mm Sherman and Panzer IV, 88 mm Tiger),
		 * the anti-tank launcher (60 mm bazooka, 88 mm Panzerschreck), the howitzer (105 mm). */
		double CalibreOf(const std::string& Weapon, const std::string& Model, const std::string& Army)
		{
			if (Weapon == "tank") return Model == "Tiger" || Model == "KingTiger" ? 88.0 : Model == "Panther" ? 75.0 : 75.0;
			if (Weapon == "at") return Army == "GER" ? 88.0 : 60.0;
			if (Weapon == "gun") return 105.0;
			if (Weapon == "mortar") return 81.0;
			return 8.0;
		}
	}

	Burst BurstOf(const std::string& Weapon, const std::string& Army, const std::string& Role)
	{
		Burst B;
		if (Weapon != "mg") return B;
		if (Army == "GER")
		{
			// The MG 42 (1,200 rounds a minute): a short burst of 7 to 10, as its crews were taught.
			B.Rounds = Role == "hmg" ? 10 : 8;
			B.PerSecond = 20.0;
		}
		else if (Army == "USA")
		{
			// The squad's BAR (about 8 a second, bursts of 3 to 5) and the battalion's tripod M1919 (about 8, bursts
			// of 5 to 6).
			B.Rounds = Role == "hmg" ? 6 : 4;
			B.PerSecond = 8.0;
		}
		else if (Army == "SOV")
		{
			B.Rounds = Role == "hmg" ? 8 : 6;
			B.PerSecond = 10.0;
		}
		else
		{
			B.Rounds = 5;
			B.PerSecond = 9.0;
		}
		return B;
	}

	std::string GroundWordOf(const std::string& Ground)
	{
		if (Ground == "woods") return "woods";
		if (IsOneOf(Ground, { "buildings", "suburb", "industry", "bridge" })) return "town";
		if (Ground == "water") return "water";
		if (Ground == "marsh") return "mud";
		return "earth";
	}

	bool Replay::Begin(const Recording& InRecording, const World& InWorld)
	{
		Rec = &InRecording;
		Here = InWorld;
		const size_t N = Rec->Rounds.size();
		Shooter.assign(N, -1);
		TargetTrack.assign(N, -1);
		EventOf.assign(N, -1);
		for (size_t I = 0; I < N; ++I)
		{
			const Round& R = Rec->Rounds[I];
			// (the man of the section who fired it and the man it struck: J and Oj, 0 the key man)
			if (R.By >= 0 && R.K >= 0) Shooter[I] = Rec->TrackIndexOf(R.By, R.K, R.J);
			if (R.On >= 0 && R.Ok >= 0) TargetTrack[I] = Rec->TrackIndexOf(R.On, R.Ok, R.Oj);
		}
		// A 'shell' event and its kill round are one blast (the contract: "a 'shell' event plus its kill round = ONE
		// blast"): the round lands where and when the event bursts, the same gun's.
		RoundOfEvent.assign(Rec->Events.size(), -1);
		FollowerBursts = 0;
		for (size_t E = 0; E < Rec->Events.size(); ++E)
		{
			const LandFeed::Event& Ev = Rec->Events[E];
			if (Ev.What != "shell") continue;
			// (a key man's kill round first: a follower's kill round is a burst of its own, drawn by Land, unless no
			// key man's round fits)
			int Best = -1;
			for (int Pass = 0; Pass < 2 && Best < 0; ++Pass)
			{
				double BestFar = 1e9;
				for (size_t I = 0; I < N; ++I)
				{
					const Round& R = Rec->Rounds[I];
					if (R.R != Result::Kill || EventOf[I] >= 0) continue;
					if ((R.Oj > 0) != (Pass == 1)) continue;
					if (std::abs(R.L - Ev.T) > 0.35) continue;
					if (Ev.By >= 0 && R.By >= 0 && Ev.By != R.By) continue;
					const double Far = std::hypot(R.X - Ev.X, R.Y - Ev.Y);
					if (Far < 1.5 && Far < BestFar) { Best = static_cast<int>(I); BestFar = Far; }
				}
			}
			RoundOfEvent[E] = Best;
			if (Best >= 0) EventOf[static_cast<size_t>(Best)] = static_cast<int>(E);
		}
		// A gun's, a tank's or a mortar's round (or a grenade, an anti-tank round) that killed a follower and that no
		// 'shell' event draws: its own burst, drawn where it landed by Land (the drill writes no shell event for it).
		for (size_t I = 0; I < N; ++I)
		{
			const Round& R = Rec->Rounds[I];
			if (R.R == Result::Kill && R.Oj > 0 && EventOf[I] < 0 && R.W != "rifle" && R.W != "mg") ++FollowerBursts;
		}
		ByLanding.resize(N);
		for (size_t I = 0; I < N; ++I) ByLanding[I] = static_cast<int>(I);
		std::stable_sort(ByLanding.begin(), ByLanding.end(), [&](int A, int B) { return Rec->Rounds[static_cast<size_t>(A)].L < Rec->Rounds[static_cast<size_t>(B)].L; });
		Falls.clear();
		for (size_t I = 0; I < Rec->Tracks.size(); ++I)
		{
			const Track& T = Rec->Tracks[I];
			if (T.Fell >= 0.0 && !T.bTwin) Falls.push_back({ T.Fell, static_cast<int>(I), T.IsVehicle() });
		}
		std::sort(Falls.begin(), Falls.end(), [](const FallRow& A, const FallRow& B) { return A.T < B.T; });
		Rewind();
		return true;
	}

	void Replay::Rewind()
	{
		NextFire = NextLand = NextEvent = NextFall = 0;
		Played = Paired = Alone = 0;
	}

	double Replay::HeightAt(double X, double Y) const
	{
		if (Here.Height) return Here.Height(X, Y);
		return Rec ? Rec->Ground.HeightAt(X, Y) : 0.0;
	}

	std::string Replay::GroundAt(double X, double Y) const
	{
		if (Here.GroundWord) return Here.GroundWord(X, Y);
		return Rec ? GroundWordOf(Rec->Ground.GroundAt(X, Y)) : std::string("earth");
	}

	Vec Replay::MuzzleOf(int TrackIndex, double T, const std::string& Weapon, Vec& OutFacing, const Vec& Toward) const
	{
		Vec At;
		if (TrackIndex >= 0 && Here.Muzzle && Here.Muzzle(TrackIndex, T, At, OutFacing)) return At;
		if (TrackIndex < 0 || !Rec)
		{
			// A shooter the player has not seen: the round comes from beyond what he can see, on the line it flew.
			OutFacing = Vec(0.0, 0.0, 0.0);
			return Toward;
		}
		const Track& Tr = Rec->Tracks[static_cast<size_t>(TrackIndex)];
		const LandFeed::Spot S = LandFeed::SpotAt(Tr, T);
		const LandFeed::Change* C = LandFeed::ChangeAt(Tr, T);
		const std::string Pose = C ? C->Pose : std::string("idle");
		const double Ground = HeightAt(S.X, S.Y);
		// Which way: toward where the round lands (a turret or a man turns to his aim; the cores lay the real muzzle).
		double Fx = Toward.X - S.X, Fy = Toward.Y - S.Y;
		const double Flat = std::hypot(Fx, Fy);
		if (Flat < 1e-6) { Fx = std::sin(S.F); Fy = std::cos(S.F); }
		else { Fx /= Flat; Fy /= Flat; }
		double Up = 0.0, Ahead = 0.6;
		if (Tr.Role == "tank") { Up = 2.2; Ahead = 4.0; }
		else if (Tr.Role == "gun") { Up = 1.2; Ahead = 2.0; }
		else if (Weapon == "mortar") { Up = 0.7; Ahead = 0.3; }
		else Up = MuzzleHeight(Pose);
		At = Vec(S.X + Fx * Ahead, S.Y + Fy * Ahead, Ground + Up);
		// A mortar and a howitzer firing far lay their tube high: the flash and smoke go up its line.
		double Lift = 0.0;
		if (Weapon == "mortar") Lift = 1.1;
		else if (Weapon == "gun") Lift = 0.35;
		OutFacing = Vec(Fx * std::cos(Lift), Fy * std::cos(Lift), std::sin(Lift)).Unit();
		return At;
	}

	Vec Replay::LandingOf(const Round& R, int Target, Vec& OutNormal) const
	{
		OutNormal = Vec(0.0, 0.0, 1.0);
		if (R.Oj > 0 && R.R != Result::Miss && Target >= 0 && Rec->Tracks[static_cast<size_t>(Target)].IsMan())
		{
			// A follower struck: a bullet lands on him, where he is when it lands; a shell bursts where the rules put it
			// (beside him), or on him should that be more than 15 m off.
			const LandFeed::Spot S = LandFeed::SpotAt(Rec->Tracks[static_cast<size_t>(Target)], R.L);
			const bool bBullet = R.W == "rifle" || R.W == "mg";
			if (bBullet || std::hypot(R.X - S.X, R.Y - S.Y) > 15.0) return Vec(S.X, S.Y, HeightAt(S.X, S.Y));
			return Vec(R.X, R.Y, HeightAt(R.X, R.Y));
		}
		const bool bOnSkin = R.R != Result::Miss && Target >= 0 && Rec->Tracks[static_cast<size_t>(Target)].IsVehicle();
		if (bOnSkin)
		{
			Vec At;
			if (Here.Skin && Here.Skin(Target, R.N, R.L, R.Fc.empty() ? std::string("front") : R.Fc, R.U, R.H, At, OutNormal)) return At;
			// Plainly: on the vehicle's middle at the rules' height up it (a tank about 2.6 m high, a gun 1.6).
			const Track& Tr = Rec->Tracks[static_cast<size_t>(Target)];
			const LandFeed::Spot S = LandFeed::SpotAt(Tr, R.L);
			const double High = Tr.Role == "tank" ? 2.6 : Tr.Role == "gun" ? 1.6 : 2.4;
			return Vec(S.X, S.Y, HeightAt(S.X, S.Y) + std::clamp(R.H, 0.05, 1.0) * High);
		}
		return Vec(R.X, R.Y, HeightAt(R.X, R.Y));
	}

	void Replay::Step(double From, double To, std::vector<Effect>& OutFx, std::vector<Noise>& OutNoise, std::vector<Loss>& OutLoss)
	{
		if (!Rec || To < From) return;
		const std::vector<Round>& Rounds = Rec->Rounds;
		// Rounds fired, in the order fired.
		// (each once, by the cursor: a frame that skips ahead still plays every round it passed, as the contract asks of
		// a long flight leaving the list before it lands)
		while (NextFire < Rounds.size() && Rounds[NextFire].T <= To)
		{
			Fire(Rounds[NextFire], static_cast<int>(NextFire), OutFx, OutNoise);
			++NextFire;
			++Played;
		}
		// Rounds landing, in the order they land.
		while (NextLand < ByLanding.size() && Rounds[static_cast<size_t>(ByLanding[NextLand])].L <= To)
		{
			const int I = ByLanding[NextLand];
			Land(Rounds[static_cast<size_t>(I)], I, OutFx, OutNoise);
			++NextLand;
		}
		// The battle's blasts and its words.
		while (NextEvent < Rec->Events.size() && Rec->Events[NextEvent].T <= To)
		{
			const LandFeed::Event& E = Rec->Events[NextEvent];
			if (E.What == "shell") Blast(E, RoundOfEvent[NextEvent], OutFx, OutNoise);
			else if (E.What == "contact" && E.Unit >= 0)
			{
				const LandFeed::Block* B = Rec->BlockOf(E.Unit);
				Noise N;
				N.T = E.T;
				N.Kind = NoiseKind::Contact;
				N.At = Vec(E.X, E.Y, HeightAt(E.X, E.Y) + 1.5);
				N.Army = B ? B->Army : std::string();
				N.Side = LandFeed::SideOf(E.Side);
				N.Track = -1;
				OutNoise.push_back(N);
			}
			else if (E.What == "broken")
			{
				Noise N;
				N.T = E.T;
				N.Kind = NoiseKind::Broken;
				N.Side = LandFeed::SideOf(E.Side);
				OutNoise.push_back(N);
			}
			++NextEvent;
		}
		// Men down and vehicles knocked out, when the rules took them.
		while (NextFall < Falls.size() && Falls[NextFall].T <= To)
		{
			const FallRow& F = Falls[NextFall];
			const Track& Tr = Rec->Tracks[static_cast<size_t>(F.Track)];
			const LandFeed::Spot S = LandFeed::SpotAt(Tr, F.T);
			Loss L;
			L.T = F.T;
			L.Track = F.Track;
			L.bVehicle = F.bVehicle;
			L.At = Vec(S.X, S.Y, HeightAt(S.X, S.Y));
			for (size_t I = 0; I < Rounds.size(); ++I)
				if (TargetTrack[I] == F.Track && Rounds[I].R == Result::Kill) { L.By = Rounds[I].W; break; }
			OutLoss.push_back(L);
			Noise N;
			N.T = F.T;
			N.Kind = F.bVehicle ? NoiseKind::KnockedOut : NoiseKind::Fall;
			N.At = L.At + Vec(0.0, 0.0, F.bVehicle ? 1.5 : 0.3);
			N.Track = F.Track;
			const LandFeed::Block* B = Rec->BlockOf(Tr.Block);
			N.Army = B ? B->Army : std::string();
			N.Side = B ? B->Side : -1;
			N.Weapon = Tr.Role;
			OutNoise.push_back(N);
			++NextFall;
		}
	}

	void Replay::Fire(const Round& R, int Index, std::vector<Effect>& OutFx, std::vector<Noise>& OutNoise)
	{
		const int S = Shooter[static_cast<size_t>(Index)];
		const int Target = TargetTrack[static_cast<size_t>(Index)];
		const LandFeed::Block* By = R.By >= 0 ? Rec->BlockOf(R.By) : nullptr;
		const std::string Army = By ? By->Army : std::string();
		const std::string Role = S >= 0 ? Rec->Tracks[static_cast<size_t>(S)].Role : std::string();
		Vec Normal;
		// Where it is going: the landing at its own time (a vehicle's skin where it strikes one).
		Round AtFire = R;
		const Vec To = LandingOf(AtFire, Target, Normal);
		Vec Facing;
		const Vec At = MuzzleOf(S, R.T, R.W, Facing, To);
		if (Facing.Length() < 0.5) Facing = (To - At).Unit();
		const std::string Ground = GroundAt(At.X, At.Y);

		Noise N;
		N.T = R.T;
		N.Kind = NoiseKind::Shot;
		N.At = At;
		N.From = At;
		N.To = To;
		N.Weapon = R.W;
		N.Army = Army;
		N.Side = By ? By->Side : -1;
		N.Track = S;
		N.Role = Role;
		N.bMiss = R.R == Result::Miss;
		N.LandsAt = R.L;

		Effect M;
		M.T = R.T;
		M.Kind = FxKind::Muzzle;
		M.At = At;
		M.Way = Facing;
		M.Ground = Ground;
		M.Round = R.N;
		if (R.W == "rifle" || R.W == "mg")
		{
			const Burst B = BurstOf(R.W, Army, Role);
			M.Name = R.W;
			Effect Sh;
			Sh.T = R.T;
			Sh.Kind = FxKind::Shots;
			Sh.At = At;
			Sh.To = To;
			Sh.Way = Facing;
			Sh.Name = R.W;
			Sh.Gun = S >= 0 ? S : 100000 + R.N;
			Sh.Rounds = B.Rounds;
			Sh.PerSecond = B.PerSecond;
			Sh.Ground = GroundAt(To.X, To.Y);
			Sh.Round = R.N;
			N.Rounds = B.Rounds;
			N.PerSecond = B.PerSecond;
			if (S >= 0) OutFx.push_back(M);
			OutFx.push_back(Sh);
		}
		else if (R.W == "grenade")
		{
			// A grenade has no flash at the hand: it is the throw (the soldiers' core) and the burst where it lands.
		}
		else
		{
			M.Name = R.W == "tank" ? "tank_gun" : R.W == "at" ? "at_gun" : R.W == "gun" ? "howitzer" : R.W == "mortar" ? "mortar" : "rifle";
			if (S >= 0) OutFx.push_back(M);
		}
		OutNoise.push_back(N);
	}

	void Replay::Land(const Round& R, int Index, std::vector<Effect>& OutFx, std::vector<Noise>& OutNoise)
	{
		if (EventOf[static_cast<size_t>(Index)] >= 0) return; // its 'shell' event draws it (Blast)
		const int Target = TargetTrack[static_cast<size_t>(Index)];
		const int S = Shooter[static_cast<size_t>(Index)];
		const LandFeed::Block* By = R.By >= 0 ? Rec->BlockOf(R.By) : nullptr;
		const LandFeed::Block* On = R.On >= 0 ? Rec->BlockOf(R.On) : nullptr;
		Vec Normal;
		const Vec At = LandingOf(R, Target, Normal);
		const std::string Ground = GroundAt(At.X, At.Y);
		const bool bOnVehicle = Target >= 0 && Rec->Tracks[static_cast<size_t>(Target)].IsVehicle() && R.R != Result::Miss;
		// Which way it came: from its muzzle (the shooter where he stood when he fired).
		Vec Facing;
		const Vec From = MuzzleOf(S, R.T, R.W, Facing, At);
		Vec Incoming = (At - From).Unit();
		if ((At - From).Length() < 0.5) Incoming = Vec(0.0, 1.0, 0.0);
		const std::string Model = By ? By->Model : std::string();
		const std::string Army = By ? By->Army : std::string();

		Noise N;
		N.T = R.L;
		N.At = At;
		N.Weapon = R.W;
		N.Army = Army;
		N.Side = By ? By->Side : -1;
		N.Track = Target;
		N.bArmour = bOnVehicle;

		Effect F;
		F.T = R.L;
		F.At = At;
		F.Ground = Ground;
		F.Round = R.N;
		F.Normal = Normal;
		F.Way = Incoming;
		F.CalibreMm = CalibreOf(R.W, Model, Army);

		if (R.W == "rifle" || R.W == "mg")
		{
			// The kit's Shots lays the strikes in the dirt. On armour small arms only rattle: sparks and the ring.
			if (bOnVehicle && R.R == Result::Glance)
			{
				F.Kind = FxKind::Glance;
				F.CalibreMm = 8.0;
				OutFx.push_back(F);
				N.Kind = NoiseKind::Glance;
				OutNoise.push_back(N);
			}
			return;
		}
		const bool bAgainstArmour = R.W == "tank" || R.W == "at";
		const bool bTargetArmour = (On && (On->Kind == "arm")) || bOnVehicle;
		if (bAgainstArmour && (bTargetArmour || bOnVehicle))
		{
			// Armour-piercing at a tank: a miss skips along the ground; a glance throws sparks and the shot off; a round
			// that goes in (worn or kill) is the bright strike on the plate.
			if (R.R == Result::Miss)
			{
				F.Kind = FxKind::Skip;
				Vec Flat = Vec(Incoming.X, Incoming.Y, 0.0);
				F.Way = Flat.Length() > 1e-6 ? Flat.Unit() : Vec(0.0, 1.0, 0.0);
				OutFx.push_back(F);
				N.Kind = NoiseKind::Landing;
				N.Size = "ap";
			}
			else if (R.R == Result::Glance)
			{
				F.Kind = FxKind::Glance;
				OutFx.push_back(F);
				N.Kind = NoiseKind::Glance;
			}
			else
			{
				F.Kind = FxKind::Plate;
				F.Name = F.CalibreMm >= 85.0 ? "gun75" : "gun75";
				F.bArmour = true;
				OutFx.push_back(F);
				N.Kind = NoiseKind::Holed;
			}
			OutNoise.push_back(N);
			return;
		}
		// High explosive: a tank's at foot or a gun, the battery's shells, the mortars' bombs, grenades.
		N.Kind = NoiseKind::Landing;
		if (R.W == "grenade")
		{
			F.Kind = FxKind::Size;
			F.Name = "grenade";
			N.Size = "grenade";
		}
		else if (R.W == "tank" || R.W == "at")
		{
			F.Kind = FxKind::Size;
			F.Name = R.W == "at" ? "light" : "gun75";
			F.bArmour = bOnVehicle;
			N.Size = R.W == "at" ? "field_gun" : "tank_he";
		}
		else
		{
			const std::string Cal = R.W == "mortar" ? std::string("mortar") : (By && !By->Cal.empty() ? By->Cal : std::string("howitzer"));
			F.Kind = FxKind::Shell;
			F.Name = Cal;
			F.bArmour = bOnVehicle;
			N.Size = LandingSizeOf(Cal);
		}
		OutFx.push_back(F);
		OutNoise.push_back(N);
	}

	void Replay::Blast(const LandFeed::Event& E, int RoundIndex, std::vector<Effect>& OutFx, std::vector<Noise>& OutNoise)
	{
		Vec Normal(0.0, 0.0, 1.0);
		Vec At(E.X, E.Y, HeightAt(E.X, E.Y));
		int Target = -1;
		if (RoundIndex >= 0)
		{
			++Paired;
			const Round& R = Rec->Rounds[static_cast<size_t>(RoundIndex)];
			Target = TargetTrack[static_cast<size_t>(RoundIndex)];
			if (E.bArmour && Target >= 0) At = LandingOf(R, Target, Normal);
		}
		else ++Alone;
		Effect F;
		F.T = E.T;
		F.Kind = FxKind::Shell;
		F.At = At;
		F.Normal = Normal;
		F.Name = E.Cal.empty() ? std::string("howitzer") : E.Cal;
		F.Ground = Here.GroundWord ? GroundAt(At.X, At.Y) : GroundWordOf(E.Ground.empty() ? Rec->Ground.GroundAt(E.X, E.Y) : E.Ground);
		F.bArmour = E.bArmour;
		F.Round = RoundIndex >= 0 ? Rec->Rounds[static_cast<size_t>(RoundIndex)].N : 0;
		OutFx.push_back(F);
		Noise N;
		N.T = E.T;
		N.Kind = NoiseKind::Landing;
		N.At = At;
		N.Size = LandingSizeOf(F.Name);
		N.bArmour = E.bArmour;
		N.Track = Target;
		const LandFeed::Block* By = E.By >= 0 ? Rec->BlockOf(E.By) : nullptr;
		N.Army = By ? By->Army : std::string();
		N.Side = By ? By->Side : -1;
		OutNoise.push_back(N);
	}
}
