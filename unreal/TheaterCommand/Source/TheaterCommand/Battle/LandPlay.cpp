#include "Battle/LandPlay.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

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
			// (a smoke round: the platoon's light mortar, the British 2-inch, the American 60 mm, the others' 50 mm)
			if (Weapon == "smoke") return Army == "UK" ? 51.0 : Army == "USA" || Army == "FRA" ? 60.0 : Army == "ITA" ? 45.0 : 50.0;
			return 8.0;
		}
	}

	Burst BurstOf(const std::string& Weapon, const std::string& Army, const std::string& Role)
	{
		Burst B;
		if (Weapon != "mg") return B;
		if (Role == "tank")
		{
			// A tank's: the Germans' MG 34 (about 15 a second), the Americans' M1919 and .50 (about 8), the Russians' DT
			// (10), the British Besa (13); her gunners' bursts a little longer than a section's.
			B.Rounds = Army == "GER" ? 9 : Army == "USA" ? 6 : Army == "SOV" ? 7 : 7;
			B.PerSecond = Army == "GER" ? 15.0 : Army == "USA" ? 8.0 : Army == "SOV" ? 10.0 : Army == "UK" ? 13.0 : 9.0;
			return B;
		}
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
		ByLanding.resize(N);
		for (size_t I = 0; I < N; ++I) ByLanding[I] = static_cast<int>(I);
		std::stable_sort(ByLanding.begin(), ByLanding.end(), [&](int A, int B) { return Rec->Rounds[static_cast<size_t>(A)].L < Rec->Rounds[static_cast<size_t>(B)].L; });
		// A 'shell' event and its kill round are one blast (the contract: "a 'shell' event plus its kill round = ONE
		// blast"): the round lands where and when the event bursts, the same gun's.
		RoundOfEvent.assign(Rec->Events.size(), -1);
		FollowerBursts = 0;
		PairBlasts(0, Rec->Events.size());
		// A gun's, a tank's or a mortar's round (or a grenade, an anti-tank round) that killed a follower and that no
		// 'shell' event draws: its own burst, drawn where it landed by Land (the drill writes no shell event for it).
		for (size_t I = 0; I < N; ++I)
		{
			const Round& R = Rec->Rounds[I];
			if (R.R == Result::Kill && R.Oj > 0 && EventOf[I] < 0 && !R.bMate && R.W != "rifle" && R.W != "mg" && R.W != "track") ++FollowerBursts;
		}
		Falls.clear();
		FallListed.assign(Rec->Tracks.size(), 0);
		for (size_t I = 0; I < Rec->Tracks.size(); ++I)
		{
			const Track& T = Rec->Tracks[I];
			if (T.Fell >= 0.0 && !T.bTwin) { Falls.push_back({ T.Fell, static_cast<int>(I), T.IsVehicle() }); FallListed[I] = 1; }
		}
		// (men down on one step in the order of their tracks: a live battle's come in that order)
		std::sort(Falls.begin(), Falls.end(), [](const FallRow& A, const FallRow& B) { return A.T != B.T ? A.T < B.T : A.Track < B.Track; });
		Rewind();
		return true;
	}

	void Replay::PairBlasts(size_t From, size_t To)
	{
		const std::vector<Round>& Rounds = Rec->Rounds;
		auto LandsBefore = [&Rounds](int I, double L) { return Rounds[static_cast<size_t>(I)].L < L; };
		for (size_t E = From; E < To && E < Rec->Events.size(); ++E)
		{
			const LandFeed::Event& Ev = Rec->Events[E];
			if (Ev.What != "shell" || RoundOfEvent[E] >= 0) continue;
			// (a key man's kill round first: a follower's kill round is a burst of its own, drawn by Land, unless no
			// key man's round fits)
			// (the rounds landing within 0.35 s of the burst, found in the landing order; on a tie of distance the
			// earlier round in the recording's order, as a search of every round in order would take it)
			const auto First = std::lower_bound(ByLanding.begin(), ByLanding.end(), Ev.T - 0.35 - 1e-9, LandsBefore);
			int Best = -1;
			for (int Pass = 0; Pass < 2 && Best < 0; ++Pass)
			{
				double BestFar = 1e9;
				for (auto It = First; It != ByLanding.end() && Rounds[static_cast<size_t>(*It)].L <= Ev.T + 0.35 + 1e-9; ++It)
				{
					const size_t I = static_cast<size_t>(*It);
					const Round& R = Rounds[I];
					if (R.R != Result::Kill || EventOf[I] >= 0) continue;
					if ((R.Oj > 0) != (Pass == 1)) continue;
					if (std::abs(R.L - Ev.T) > 0.35) continue;
					if (Ev.By >= 0 && R.By >= 0 && Ev.By != R.By) continue;
					const double Far = std::hypot(R.X - Ev.X, R.Y - Ev.Y);
					if (Far < 1.5 && (Far < BestFar || (Far == BestFar && static_cast<int>(I) < Best))) { Best = static_cast<int>(I); BestFar = Far; }
				}
			}
			RoundOfEvent[E] = Best;
			if (Best >= 0) EventOf[static_cast<size_t>(Best)] = static_cast<int>(E);
		}
	}

	void Replay::Extend(const LandFeed::Grown& G)
	{
		if (!Rec) return;
		const size_t N = Rec->Rounds.size();
		if (G.bReset || G.RoundsFrom < Shooter.size() || G.EventsFrom < RoundOfEvent.size())
		{
			// Placed again before the clock ran (or the rounds sorted again): index it all afresh, and on from where it had
			// played to without playing it again.
			const double To = G.bReset ? -1.0 : PlayedTo;
			Begin(*Rec, Here);
			if (To >= 0.0)
			{
				while (NextFire < N && Rec->Rounds[NextFire].T <= To) ++NextFire;
				while (NextLand < ByLanding.size() && Rec->Rounds[static_cast<size_t>(ByLanding[NextLand])].L <= To) ++NextLand;
				while (NextEvent < Rec->Events.size() && Rec->Events[NextEvent].T <= To) ++NextEvent;
				while (NextFall < Falls.size() && Falls[NextFall].T <= To) ++NextFall;
				PlayedTo = To;
			}
			return;
		}
		// The new rounds: their shooters and targets, and their landings in order (all after what has landed).
		Shooter.resize(N, -1);
		TargetTrack.resize(N, -1);
		EventOf.resize(N, -1);
		for (size_t I = G.RoundsFrom; I < N; ++I)
		{
			const Round& R = Rec->Rounds[I];
			if (R.By >= 0 && R.K >= 0) Shooter[I] = Rec->TrackIndexOf(R.By, R.K, R.J);
			if (R.On >= 0 && R.Ok >= 0) TargetTrack[I] = Rec->TrackIndexOf(R.On, R.Ok, R.Oj);
			const auto At = std::upper_bound(ByLanding.begin() + static_cast<std::ptrdiff_t>(NextLand), ByLanding.end(), R.L,
				[this](double L, int J) { return L < Rec->Rounds[static_cast<size_t>(J)].L; });
			ByLanding.insert(At, static_cast<int>(I));
		}
		// The blasts: the new 'shell' events, and any not yet played still unpaired (its round may come a delta after it).
		RoundOfEvent.resize(Rec->Events.size(), -1);
		PairBlasts(NextEvent, Rec->Events.size());
		for (size_t I = G.RoundsFrom; I < N; ++I)
		{
			const Round& R = Rec->Rounds[I];
			if (R.R == Result::Kill && R.Oj > 0 && EventOf[I] < 0 && !R.bMate && R.W != "rifle" && R.W != "mg" && R.W != "track") ++FollowerBursts;
		}
		// The new falls, in order after those played.
		FallListed.resize(Rec->Tracks.size(), 0);
		for (const int K : G.Fell)
		{
			const Track& T = Rec->Tracks[static_cast<size_t>(K)];
			if (T.Fell < 0.0 || T.bTwin || FallListed[static_cast<size_t>(K)]) continue;
			FallListed[static_cast<size_t>(K)] = 1;
			const FallRow Row { T.Fell, K, T.IsVehicle() };
			const auto At = std::upper_bound(Falls.begin() + static_cast<std::ptrdiff_t>(NextFall), Falls.end(), Row.T,
				[](double Tm, const FallRow& F) { return Tm < F.T; });
			Falls.insert(At, Row);
		}
	}

	void Replay::Rewind()
	{
		NextFire = NextLand = NextEvent = NextFall = 0;
		Played = Paired = Alone = 0;
		PlayedTo = -1.0;
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

	std::string Replay::TankMg(int TrackIndex, double T, int RoundN, const Vec& Toward, const std::string& Gun) const
	{
		// (no roof gun drawn on her, World::RoofGun: none fired)
		const bool bRoof = !Here.RoofGun || TrackIndex < 0 || Here.RoofGun(TrackIndex);
		if (Gun == "roof") return bRoof ? "roof_mg" : "coax";
		if (Gun == "coax") return "coax";
		if (!Rec || TrackIndex < 0) return Gun == "hull" ? "hull_mg" : "coax";
		const Track& Tr = Rec->Tracks[static_cast<size_t>(TrackIndex)];
		// (the hull gunner's ball mount swings some 25 degrees either side of her bow: a burst the rules gave him at a
		// mark wider of her bow, as the film has her facing then, is the turret's coaxial instead, which turns to it)
		const auto InBowArc = [&Tr, T, &Toward]()
		{
			const LandFeed::Spot S = LandFeed::SpotAt(Tr, T);
			const double Bearing = std::atan2(Toward.X - S.X, Toward.Y - S.Y);
			return std::fabs(std::remainder(Bearing - S.F, 2.0 * 3.14159265358979)) < 25.0 * 3.14159265358979 / 180.0;
		};
		if (Gun == "hull") return InBowArc() ? "hull_mg" : "coax";
		const LandFeed::Change* C = LandFeed::ChangeAt(Tr, T);
		const LandFeed::Block* B = Rec->BlockOf(Tr.Block);
		const std::string Army = B ? B->Army : std::string();
		// (a burst's own roll, the same each time it is asked)
		uint64_t H = static_cast<uint64_t>(RoundN + 1) * 0x9E3779B97F4A7C15ull ^ static_cast<uint64_t>(TrackIndex) * 0xC2B2AE3D27D4EB4Full;
		H ^= H >> 29;
		H *= 0xBF58476D1CE4E5B9ull;
		H ^= H >> 32;
		const int Roll = static_cast<int>(H % 6);
		if (bRoof && C && !C->bButtoned && (Army == "USA" || Army == "GER") && Roll < 2) return "roof_mg";
		if (Roll % 2 == 1 && InBowArc()) return "hull_mg";
		return "coax";
	}

	Vec Replay::MuzzleOf(int TrackIndex, double T, const std::string& Weapon, Vec& OutFacing, const Vec& Toward, int RoundN, const std::string& Gun) const
	{
		Vec At;
		const bool bTankMg = TrackIndex >= 0 && Rec && Weapon == "mg" && Rec->Tracks[static_cast<size_t>(TrackIndex)].Role == "tank";
		const std::string Socket = bTankMg ? TankMg(TrackIndex, T, RoundN, Toward, Gun) : std::string();
		if (TrackIndex >= 0 && Here.Muzzle && Here.Muzzle(TrackIndex, T, Socket, At, OutFacing))
		{
			// (a machine gun swings to its mark: the burst leaves toward it)
			if (bTankMg && (Toward - At).Length() > 0.5) OutFacing = (Toward - At).Unit();
			return At;
		}
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
		if (bTankMg) { Up = Socket == "roof_mg" ? 2.9 : Socket == "hull_mg" ? 1.6 : 2.1; Ahead = Socket == "roof_mg" ? 0.0 : Socket == "hull_mg" ? 2.6 : 1.4; }
		else if (Tr.Role == "tank") { Up = 2.2; Ahead = 4.0; }
		else if (Tr.Role == "gun") { Up = 1.2; Ahead = 2.0; }
		else if (Weapon == "mortar" || Weapon == "smoke") { Up = 0.7; Ahead = 0.3; } // (a smoke round: the light mortar's)
		else Up = MuzzleHeight(Pose);
		At = Vec(S.X + Fx * Ahead, S.Y + Fy * Ahead, Ground + Up);
		// A mortar and a howitzer firing far lay their tube high: the flash and smoke go up its line.
		double Lift = 0.0;
		if (Weapon == "mortar" || Weapon == "smoke") Lift = 1.1;
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
		PlayedTo = std::max(PlayedTo, To);
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
		if (R.bMate) return; // (shell blasts: the round before it fired the one shot)
		if (R.W == "track") return; // (infantry with tanks: a man run over by a tank; no shot, his fall is the look)
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
		const Vec At = MuzzleOf(S, R.T, R.W, Facing, To, R.N, R.G);
		if (Facing.Length() < 0.5) Facing = (To - At).Unit();
		const std::string Ground = GroundAt(At.X, At.Y);

		Noise N;
		N.T = R.T;
		N.Kind = NoiseKind::Shot;
		N.At = At;
		N.From = At;
		N.To = To;
		N.Weapon = R.W == "smoke" ? std::string("mortar") : R.W; // (a smoke round is fired from the light mortar: its sound)
		N.Army = Army;
		N.Side = By ? By->Side : -1;
		N.Track = S;
		N.Role = Role;
		N.bSniper = R.bSn;
		N.bHidden = R.bSn && R.bHd;
		// (the same gun the muzzle was put on: TankMg rolls the same for the same round)
		if (R.W == "mg" && Role == "tank") N.Mount = TankMg(S, R.T, R.N, To, R.G);
		N.bMiss = R.R == Result::Miss;
		N.LandsAt = R.L;

		Effect M;
		M.T = R.T;
		M.Kind = FxKind::Muzzle;
		M.At = At;
		M.Way = Facing;
		M.Ground = Ground;
		M.Round = R.N;
		// Effects (R2): the shooter's own gun's look (-TheaterLandGunFlash): "mg:star", the MG 42's; none, as before
		int LookRounds = 0;
		double LookRate = 0.0;
		const std::string Look = Here.GunLook && S >= 0 ? Here.GunLook(S, R.W, Army, Role, LookRounds, LookRate) : std::string();
		if (R.W == "rifle" || R.W == "mg")
		{
			const Burst B = BurstOf(R.W, Army, Role);
			M.Name = Look.empty() ? R.W : R.W + ":" + Look;
			Effect Sh;
			Sh.T = R.T;
			Sh.Kind = FxKind::Shots;
			Sh.At = At;
			Sh.To = To;
			Sh.Way = Facing;
			Sh.Name = M.Name;
			Sh.Gun = S >= 0 ? S : 100000 + R.N;
			Sh.Rounds = LookRounds > 0 ? std::min(LookRounds, 40) : B.Rounds; // (Effects (R2): a sub-machine gun's burst, seen; heard by his clip)
			Sh.PerSecond = LookRate > 0.0 ? std::min(LookRate, 30.0) : B.PerSecond;
			Sh.Ground = GroundAt(To.X, To.Y);
			Sh.Round = R.N;
			N.Rounds = B.Rounds;
			N.PerSecond = B.PerSecond;
			// (a support team's gun on its mount, release 2's "teamguns": the scene gives her own burst, heard as long as
			// it is seen; a sub-machine gun's is heard by his clip)
			if (Role == "hmg" && LookRounds > 0)
			{
				N.Rounds = Sh.Rounds;
				N.PerSecond = Sh.PerSecond;
			}
			if (S >= 0) OutFx.push_back(M);
			OutFx.push_back(Sh);
		}
		else if (R.W == "grenade")
		{
			// A grenade has no flash at the hand: it is the throw (the soldiers' core) and the burst where it lands.
		}
		else
		{
			M.Name = R.W == "tank" ? "tank_gun" : R.W == "at" ? "at_gun" : R.W == "gun" ? "howitzer" : R.W == "mortar" || R.W == "smoke" ? "mortar" : "rifle";
			if (!Look.empty()) M.Name += ":" + Look; // (Effects (R2): a muzzle brake's side jets, "tank_gun:brake")
			if (S >= 0) OutFx.push_back(M);
		}
		OutNoise.push_back(N);
	}

	void Replay::Land(const Round& R, int Index, std::vector<Effect>& OutFx, std::vector<Noise>& OutNoise)
	{
		if (EventOf[static_cast<size_t>(Index)] >= 0) return; // its 'shell' event draws it (Blast)
		if (R.bMate) return; // (shell blasts: struck by the burst before it, drawn once by that round or its event)
		if (R.W == "track") return; // (run over: nothing lands)
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
		const Vec From = MuzzleOf(S, R.T, R.W, Facing, At, R.N, R.G);
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
		// A smoke round (the drill book's screens, 22 November) is not high explosive: no burst of earth, no hole, no
		// blast's noise. Its screen: a phosphorus round's white burst for the Americans' 60 mm, a burning bomb's gush for
		// the others' light mortars.
		if (R.W == "smoke")
		{
			F.Kind = FxKind::Screen;
			F.Name = Army == "USA" ? "wp" : "canister";
			F.Hang = R.Hang;
			OutFx.push_back(F);
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
