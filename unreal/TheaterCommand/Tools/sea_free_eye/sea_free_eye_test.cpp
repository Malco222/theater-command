// The sea free eye's cloud test (the sea free eye plan, section 5; Tools/sea_free_eye/run.sh builds and runs it): the
// sea's side of the free camera (Battle/SeaFreeEyeGlue, the very code the game runs) over the camera thread's core
// (Battle/FreeEye), checked by hand-made cases, played through every sea recording as the scene plays it (the show at
// the battle's clock, the camera on real seconds, at 60 and 24 frames a second, through hitches and jitter, the battle's
// pace a quarter, one and four, standing still), over today's still face, a moving 12 m sea and the real gale, and the
// scene's wiring read as text (Battle/SeaBattle.cpp, the switches). Plain C++, no engine, no clocks: the same output on
// every run and with both compilers.
//
// Usage: sea_free_eye_test <Source/TheaterCommand> <director.sha> [--quick] recording.json ...
//   --quick plays each recording's first minutes only (the breakages' runs: Tools/sea_free_eye/breakages.sh).
// Prints a line a check and "N checks, M failed"; exits 1 on any failure, 2 when it cannot read what it was given.

#include "Battle/FleetWaves.h"
#include "Battle/FreeEye.h"
#include "Battle/SeaFeed.h"
#include "Battle/SeaFreeEyeGlue.h"
#include "Battle/SeaLookSwitches.h"
#include "Battle/SeaShow.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace
{
	using SeaFreeEye::KeyName;
	using SeaFreeEye::Vec;
	using SeaShow::Fate;
	using SeaShow::ShipOut;

	constexpr double Rad = 3.14159265358979323846 / 180.0;
	/** The agreed numbers, written here again, never read from the glue: a breakage of the glue's own must show. */
	constexpr double AgreedCeiling = 1500.0, AgreedClear = 3.0, HardClear = 0.5, LensMostPerSecond = 40.0;

	int Checks = 0, Failed = 0;

	void Check(const std::string& Id, const std::string& What, bool bOk, const std::string& Seen)
	{
		++Checks;
		Failed += bOk ? 0 : 1;
		std::printf("  %s %-4s %s: %s\n", bOk ? "ok  " : "FAIL", Id.c_str(), What.c_str(), Seen.c_str());
	}

	std::string Num(double V, int Places)
	{
		char Text[64] = {};
		std::snprintf(Text, sizeof(Text), "%.*f", Places, std::isfinite(V) ? V : 0.0);
		return std::string(std::isfinite(V) ? Text : "not finite");
	}

	std::string ReadAll(const std::string& Path)
	{
		std::ifstream File(Path, std::ios::binary);
		std::stringstream Text;
		Text << File.rdbuf();
		return Text.str();
	}

	// ---- SHA-256 (the director's text pins, check C9) ----------------------------------------------------------------

	std::string Sha256(const std::string& Text)
	{
		static const uint32_t K[64] = { 0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98,
			0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
			0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351,
			0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
			0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3,
			0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2 };
		uint32_t H[8] = { 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 };
		std::string M = Text;
		const uint64_t Bits = static_cast<uint64_t>(Text.size()) * 8u;
		M.push_back(static_cast<char>(0x80));
		while (M.size() % 64 != 56)
		{
			M.push_back('\0');
		}
		for (int I = 7; I >= 0; --I)
		{
			M.push_back(static_cast<char>((Bits >> (I * 8)) & 0xffu));
		}
		auto Rot = [](uint32_t X, int N) { return (X >> N) | (X << (32 - N)); };
		for (size_t B = 0; B < M.size(); B += 64)
		{
			uint32_t W[64];
			for (int I = 0; I < 16; ++I)
			{
				const unsigned char* P = reinterpret_cast<const unsigned char*>(M.data() + B + 4 * I);
				W[I] = (uint32_t(P[0]) << 24) | (uint32_t(P[1]) << 16) | (uint32_t(P[2]) << 8) | uint32_t(P[3]);
			}
			for (int I = 16; I < 64; ++I)
			{
				const uint32_t S0 = Rot(W[I - 15], 7) ^ Rot(W[I - 15], 18) ^ (W[I - 15] >> 3);
				const uint32_t S1 = Rot(W[I - 2], 17) ^ Rot(W[I - 2], 19) ^ (W[I - 2] >> 10);
				W[I] = W[I - 16] + S0 + W[I - 7] + S1;
			}
			uint32_t A = H[0], Bv = H[1], C = H[2], D = H[3], E = H[4], F = H[5], G = H[6], Hv = H[7];
			for (int I = 0; I < 64; ++I)
			{
				const uint32_t S1 = Rot(E, 6) ^ Rot(E, 11) ^ Rot(E, 25);
				const uint32_t Ch = (E & F) ^ (~E & G);
				const uint32_t T1 = Hv + S1 + Ch + K[I] + W[I];
				const uint32_t S0 = Rot(A, 2) ^ Rot(A, 13) ^ Rot(A, 22);
				const uint32_t Maj = (A & Bv) ^ (A & C) ^ (Bv & C);
				const uint32_t T2 = S0 + Maj;
				Hv = G;
				G = F;
				F = E;
				E = D + T1;
				D = C;
				C = Bv;
				Bv = A;
				A = T1 + T2;
			}
			H[0] += A;
			H[1] += Bv;
			H[2] += C;
			H[3] += D;
			H[4] += E;
			H[5] += F;
			H[6] += G;
			H[7] += Hv;
		}
		std::string Hex;
		for (const uint32_t Word : H)
		{
			char Part[9] = {};
			std::snprintf(Part, sizeof(Part), "%08x", static_cast<unsigned>(Word));
			Hex += Part;
		}
		return Hex;
	}

	// ---- A digest of everything the free eye gives (check C16: bit for bit) ----------------------------------------

	struct Digest
	{
		uint64_t H = 1469598103934665603ull;
		void Bytes(const void* P, size_t N)
		{
			const unsigned char* C = static_cast<const unsigned char*>(P);
			for (size_t I = 0; I < N; ++I)
			{
				H = (H ^ C[I]) * 1099511628211ull;
			}
		}
		void D(double V) { Bytes(&V, sizeof(V)); }
		void I(long long V) { Bytes(&V, sizeof(V)); }
		void V(const Vec& P)
		{
			D(P.X);
			D(P.Y);
			D(P.Z);
		}
		void S(const std::string& Text) { Bytes(Text.data(), Text.size()); I(static_cast<long long>(Text.size())); }
	};

	// ---- The test's own rules, written again (never the glue's) ------------------------------------------------------

	/** Found and in the fight (the agreement: never a ship not found, sinking or gone). */
	bool Pickable(const ShipOut& Ship)
	{
		return Ship.bShown && (Ship.Doing == Fate::Line || Ship.Doing == Fate::Limping);
	}

	const ShipOut* Named(const std::vector<ShipOut>& Ships, int Key)
	{
		for (const ShipOut& Ship : Ships)
		{
			if (Ship.Key == Key)
			{
				return &Ship;
			}
		}
		return nullptr;
	}

	Vec PlaceOf(const ShipOut& Ship)
	{
		return Vec{ Ship.At.X, Ship.At.Y, Ship.At.Z + Ship.Heave };
	}

	/** The Tab order counted again: his side first, then the enemy; LineAt (none last), piece, slot; found ships only. */
	std::vector<int> Recount(const std::vector<ShipOut>& Ships, int Mine)
	{
		struct Row
		{
			int Side, NoLine, Line, Piece, Slot, Key;
		};
		std::vector<Row> Rows;
		for (const ShipOut& Ship : Ships)
		{
			if (Pickable(Ship))
			{
				Rows.push_back(Row{ Ship.Side == Mine ? 0 : 1, Ship.LineAt < 0 ? 1 : 0, Ship.LineAt, Ship.Piece, Ship.Slot, Ship.Key });
			}
		}
		std::sort(Rows.begin(), Rows.end(), [](const Row& A, const Row& B)
			{
				if (A.Side != B.Side) return A.Side < B.Side;
				if (A.NoLine != B.NoLine) return A.NoLine < B.NoLine;
				if (A.Line != B.Line) return A.Line < B.Line;
				if (A.Piece != B.Piece) return A.Piece < B.Piece;
				if (A.Slot != B.Slot) return A.Slot < B.Slot;
				return A.Key < B.Key;
			});
		std::vector<int> Keys;
		for (const Row& R : Rows)
		{
			Keys.push_back(R.Key);
		}
		return Keys;
	}

	/** A division counted again: the piece's found ships in line by slot, and their middle. */
	std::vector<int> MembersOf(const std::vector<ShipOut>& Ships, int Piece, Vec* Middle)
	{
		std::vector<std::pair<std::pair<int, int>, const ShipOut*>> In;
		for (const ShipOut& Ship : Ships)
		{
			if (Ship.Piece == Piece && Ship.bShown && Ship.Doing == Fate::Line)
			{
				In.push_back({ { Ship.Slot, Ship.Key }, &Ship });
			}
		}
		std::sort(In.begin(), In.end(), [](const auto& A, const auto& B) { return A.first < B.first; });
		std::vector<int> Keys;
		Vec Sum;
		for (const auto& One : In)
		{
			Keys.push_back(One.second->Key);
			Sum += PlaceOf(*One.second);
		}
		if (Middle)
		{
			*Middle = In.empty() ? Vec{} : Sum * (1.0 / static_cast<double>(In.size()));
		}
		return Keys;
	}

	double CeilingOf(bool bDeck, double Base)
	{
		return !bDeck ? AgreedCeiling : std::min(AgreedCeiling, std::max(300.0, Base - 150.0));
	}

	double Angle(const Vec& A, const Vec& B)
	{
		const Vec U = A.Unit(), W = B.Unit();
		const double C = std::clamp(U.X * W.X + U.Y * W.Y + U.Z * W.Z, -1.0, 1.0);
		return std::acos(C);
	}

	// ---- The floors ------------------------------------------------------------------------------------------------

	/** An analytic moving sea, three trains whose crests meet at 12 m (and troughs at -12 m), on its own clock. */
	struct MovingSea
	{
		double T = 0.0;
	};
	double MovingSeaAt(const void* Context, double X, double Y)
	{
		const double T = static_cast<const MovingSea*>(Context)->T;
		return 6.0 * std::sin(0.0349 * X + 0.0120 * Y - 0.55 * T) + 4.0 * std::sin(0.0291 * Y - 0.0410 * X + 0.71 * T + 1.3) +
			2.0 * std::sin(0.1100 * X + 0.0700 * Y - 1.20 * T + 0.4);
	}

	/** The real gale (FleetWaves under GaleCap), its Near built for each frame, as the swell's lane will hand it. */
	struct GaleFloor
	{
		FleetWaves::Sea S;
		FleetWaves::Drawn D;
		FleetWaves::Near N;
		bool bD = false;
	};
	double GaleAt(const void* Context, double X, double Y)
	{
		return FleetWaves::SampleNear(static_cast<const GaleFloor*>(Context)->N, X, Y);
	}

	// ---- A rig for the hand-made cases -----------------------------------------------------------------------------

	ShipOut MakeShip(int Key, int Piece, int Side, int Slot, double X, double Y, double Heading, int LineAt = 0)
	{
		ShipOut S;
		S.Key = Key;
		S.Piece = Piece;
		S.Side = Side;
		S.Slot = Slot;
		S.At.X = X;
		S.At.Y = Y;
		S.At.Z = 0.0;
		S.Heading = Heading;
		S.Speed = 8.0;
		S.Long = 150.0;
		S.Beam = 18.0;
		S.LineAt = LineAt;
		return S;
	}

	struct Rig
	{
		SeaFreeEye::State St;
		FreeEye::Eye Free;
		SeaFreeEye::SeaFloor Floor;
		std::vector<ShipOut> Ships;
		SeaFreeEye::Scene In;
		SeaFreeEye::Shot Last;
		explicit Rig(double Aspect = 16.0 / 9.0)
		{
			Free.Set = SeaFreeEye::SeaSettings(Aspect);
			In.Ships = &Ships;
			In.Floor = &Floor;
			In.BoardSize = 20000.0;
			In.Aspect = Aspect;
			In.DirEye = Vec{ 9000.0, 9000.0, 120.0 };
			In.DirLook = Vec{ 9600.0, 9800.0, 0.0 };
			In.DirLens = 40.0;
			In.MySide = 0;
		}
		SeaFreeEye::Shot Frame(double Dt)
		{
			In.RealDt = Dt;
			Last = SeaFreeEye::Step(St, Free, In);
			return Last;
		}
		void Key(KeyName K, bool bDown, bool bShift = false, bool bRepeat = false) { SeaFreeEye::KeyEvent(St.H, K, bDown, bShift, bRepeat); }
		void Run(double Seconds, double Dt = 1.0 / 60.0)
		{
			for (double T = 0.0; T < Seconds - 1e-9; T += Dt)
			{
				Frame(Dt);
			}
		}
		void Move(double Dt, double Pace)
		{
			for (ShipOut& Ship : Ships)
			{
				Ship.At.X += std::sin(Ship.Heading * Rad) * Ship.Speed * Dt * Pace;
				Ship.At.Y += std::cos(Ship.Heading * Rad) * Ship.Speed * Dt * Pace;
			}
		}
		SeaFreeEye::Ray RayTo(const Vec& At) const
		{
			const Vec From = Last.bFree ? Last.Eye : In.DirEye;
			return SeaFreeEye::Ray{ From, At - From };
		}
	};

	// ---- The recordings, played through the show as the scene plays it -----------------------------------------------

	struct Slim
	{
		int Key = 0, Piece = 0, Side = 0, Slot = 0, LineAt = -1;
		double X = 0.0, Y = 0.0, Z = 0.0, Heading = 0.0, Heave = 0.0, Speed = 0.0, Long = 0.0, Beam = 0.0;
		Fate Doing = Fate::Line;
		bool bShown = true;
	};

	struct Played
	{
		std::string Name;
		double BeginT = 0.0, EndT = 0.0, Size = 20000.0;
		int Mine = 0;
		/** The show's ships every quarter second of the battle from BeginT. */
		std::vector<std::vector<Slim>> Snaps;
		int HiddenSteps = 0, MostShips = 0;
	};

	constexpr double SnapStep = 0.25;

	bool Load(const std::string& Path, Played& Out)
	{
		SeaFeed::Recording Rec;
		std::string Error;
		const std::string Text = ReadAll(Path);
		if (Text.empty() || !SeaFeed::ReadRecording(Text, Rec, Error) || Rec.Views.empty())
		{
			std::printf("%s: %s\n", Path.c_str(), Error.empty() ? "nothing read" : Error.c_str());
			return false;
		}
		const size_t Slash = Path.find_last_of('/');
		Out.Name = Path.substr(Slash == std::string::npos ? 0 : Slash + 1);
		SeaShow::Show Live(Rec.Seed ? Rec.Seed : 239);
		Live.SetField(Rec.Board);
		const int Seen = SeaFeed::SideOf(Rec.Views.front().Head.Me);
		Live.SetMine(Seen);
		Out.Mine = Seen == 1 ? 1 : 0; // (as the scene's: SeaFeed::SideOf of the first view, the views' "me")
		Out.Size = Rec.Board.Size;
		Out.BeginT = Rec.Views.front().Head.T;
		Out.EndT = Rec.Views.back().Head.T;
		size_t Taken = 0;
		auto TakeNext = [&]()
		{
			const SeaFeed::Frame& V = Rec.Views[Taken++];
			Live.Take(V.Head.T, V.Pieces, V.Events, V.bZones ? &V.Zones : nullptr, V.Head.Contact);
		};
		TakeNext();
		for (int K = 0; Out.BeginT + K * SnapStep <= Out.EndT + 1e-6; ++K)
		{
			const double T = Out.BeginT + K * SnapStep;
			while (Taken < Rec.Views.size() && Rec.Views[Taken - 1].Head.T <= T + 1e-9)
			{
				TakeNext();
			}
			Live.Advance(T);
			Live.Effects.clear();
			std::vector<Slim> Now;
			bool bHidden = false;
			for (const ShipOut& Ship : Live.Ships())
			{
				Slim S;
				S.Key = Ship.Key;
				S.Piece = Ship.Piece;
				S.Side = Ship.Side;
				S.Slot = Ship.Slot;
				S.LineAt = Ship.LineAt;
				S.X = Ship.At.X;
				S.Y = Ship.At.Y;
				S.Z = Ship.At.Z;
				S.Heading = Ship.Heading;
				S.Heave = Ship.Heave;
				S.Speed = Ship.Speed;
				S.Long = Ship.Long;
				S.Beam = Ship.Beam;
				S.Doing = Ship.Doing;
				S.bShown = Ship.bShown;
				bHidden = bHidden || !Ship.bShown;
				Now.push_back(S);
			}
			Out.HiddenSteps += bHidden ? 1 : 0;
			Out.MostShips = std::max(Out.MostShips, static_cast<int>(Now.size()));
			Out.Snaps.push_back(std::move(Now));
		}
		return !Out.Snaps.empty();
	}

	/** The ships at battle time T as the scene would draw them (each place eased between the show's quarter seconds);
	 * bHidden false: every ship not found deleted (check C16). */
	void ShipsAt(const Played& P, double T, std::vector<ShipOut>& Out, bool bHidden)
	{
		Out.clear();
		const double F = std::clamp((T - P.BeginT) / SnapStep, 0.0, static_cast<double>(P.Snaps.size() - 1));
		const size_t K = static_cast<size_t>(F);
		const double U = F - static_cast<double>(K);
		const std::vector<Slim>& A = P.Snaps[K];
		const std::vector<Slim>* B = K + 1 < P.Snaps.size() ? &P.Snaps[K + 1] : nullptr;
		for (size_t I = 0; I < A.size(); ++I)
		{
			const Slim& S = A[I];
			if (!bHidden && !S.bShown)
			{
				continue;
			}
			const Slim* Next = nullptr;
			if (B && U > 0.0)
			{
				if (I < B->size() && (*B)[I].Key == S.Key)
				{
					Next = &(*B)[I];
				}
				else
				{
					for (const Slim& Other : *B)
					{
						Next = Other.Key == S.Key ? &Other : Next;
					}
				}
			}
			ShipOut O;
			O.Key = S.Key;
			O.Piece = S.Piece;
			O.Side = S.Side;
			O.Slot = S.Slot;
			O.LineAt = S.LineAt;
			O.At.X = S.X;
			O.At.Y = S.Y;
			O.At.Z = S.Z;
			O.Heading = S.Heading;
			O.Heave = S.Heave;
			O.Speed = S.Speed;
			O.Long = S.Long;
			O.Beam = S.Beam;
			O.Doing = S.Doing;
			O.bShown = S.bShown;
			if (Next)
			{
				O.At.X += (Next->X - S.X) * U;
				O.At.Y += (Next->Y - S.Y) * U;
				O.At.Z += (Next->Z - S.Z) * U;
				O.Heave += (Next->Heave - S.Heave) * U;
				const double Turn = std::remainder(Next->Heading - S.Heading, 360.0);
				O.Heading = S.Heading + Turn * U;
			}
			Out.push_back(O);
		}
	}

	/** A stand-in director: a shot every 12 battle seconds on a ship (of all of them, as the director's plan is made from
	 * the whole recording: the same in both of check C16's plays), its eye 150 m to 2.5 km off and 20 to 900 m up, its lens
	 * 15 to 85 degrees, drifting; one shot in seven under the water beside her (his under windows). */
	struct DirShot
	{
		Vec Eye, Look;
		double Lens = 50.0;
		bool bUnder = false;
	};
	uint64_t Mix(uint64_t X)
	{
		X += 0x9E3779B97F4A7C15ull;
		X = (X ^ (X >> 30)) * 0xBF58476D1CE4E5B9ull;
		X = (X ^ (X >> 27)) * 0x94D049BB133111EBull;
		return X ^ (X >> 31);
	}
	DirShot DirectorAt(const Played& P, double T, const std::vector<ShipOut>& All)
	{
		DirShot Out;
		const long long Shot = static_cast<long long>(std::floor((T - P.BeginT) / 12.0));
		const uint64_t H = Mix(static_cast<uint64_t>(Shot) * 7919ull + 17ull);
		Vec Subject{ 0.5 * P.Size, 0.5 * P.Size, 0.0 };
		if (!All.empty())
		{
			Subject = PlaceOf(All[H % All.size()]);
		}
		const double Around = static_cast<double>((H >> 8) % 360) * Rad;
		const double Since = T - (P.BeginT + 12.0 * static_cast<double>(Shot));
		Out.bUnder = Shot % 7 == 3;
		if (Out.bUnder)
		{
			Out.Eye = Subject + Vec{ std::sin(Around) * 60.0, std::cos(Around) * 60.0, -8.0 };
			Out.Lens = 70.0;
		}
		else
		{
			const double Far = 150.0 + static_cast<double>((H >> 16) % 2350), Up = 20.0 + static_cast<double>((H >> 28) % 880);
			Out.Eye = Subject + Vec{ std::sin(Around) * Far + 1.5 * Since, std::cos(Around) * Far + 0.7 * Since, 0.0 };
			Out.Eye.Z = Up;
			Out.Lens = 15.0 + static_cast<double>((H >> 40) % 71);
		}
		Out.Look = Subject;
		return Out;
	}

	// ---- His hands, seeded -------------------------------------------------------------------------------------------

	struct Player
	{
		uint64_t S;
		explicit Player(uint64_t Seed) : S(Mix(Seed) | 1u) {}
		double U()
		{
			S = S * 6364136223846793005ull + 1442695040888963407ull;
			return static_cast<double>(S >> 11) * (1.0 / 9007199254740992.0);
		}
		struct Down
		{
			KeyName Key;
			bool bShift;
			double Until, Repeat;
		};
		std::vector<Down> Held;
		double Clock = 0.0;
		double NextHold = 0.3, NextWheel = 1.5, NextPad = 17.0, NextClick = 2.5, NextJ = 5.0, NextTab = 3.5, NextC = 50.0, NextU = 75.0;
		/** What the hands did this frame that the test follows. */
		bool bUnder = false, bUnderShift = false, bPressed = false, bC = false;
		int AimedHidden = -1;
	};

	/** One frame of his hands: keys through the glue's KeyEvent (with repeats, and Shift changed while held), the wheel, the
	 * trackpad, clicks along rays (at found ships, straight at hidden boats, at the water), J, Tab, C, U. */
	void Act(Player& Pl, SeaFreeEye::State& St, double Dt, const Vec& Eye, const std::vector<ShipOut>& All, bool bDiving)
	{
		static const KeyName HeldKeys[] = { KeyName::W, KeyName::A, KeyName::S, KeyName::D, KeyName::Up, KeyName::Down, KeyName::Left, KeyName::Right, KeyName::Q,
			KeyName::E, KeyName::Q, KeyName::W };
		SeaFreeEye::Hands& H = St.H;
		Pl.Clock += Dt;
		Pl.bUnder = Pl.bUnderShift = Pl.bPressed = Pl.bC = false;
		Pl.AimedHidden = -1;
		for (size_t K = 0; K < Pl.Held.size();)
		{
			Player::Down& One = Pl.Held[K];
			if (One.Until <= Pl.Clock)
			{
				SeaFreeEye::KeyEvent(H, One.Key, false, One.bShift, false);
				Pl.Held.erase(Pl.Held.begin() + static_cast<long>(K));
				continue;
			}
			if (One.Repeat <= Pl.Clock)
			{
				// (a repeat, Shift perhaps let go or pressed meanwhile: the latch holds what the press meant)
				SeaFreeEye::KeyEvent(H, One.Key, true, Pl.U() < 0.5, true);
				One.Repeat += 0.1;
			}
			++K;
		}
		if (Pl.Clock >= Pl.NextHold)
		{
			const KeyName Key = HeldKeys[static_cast<size_t>(Pl.U() * 12.0) % 12];
			bool bFree = true;
			for (const Player::Down& One : Pl.Held)
			{
				bFree = bFree && One.Key != Key;
			}
			if (bFree)
			{
				const bool bShift = Pl.U() < (Key == KeyName::Q ? 0.6 : 0.3);
				SeaFreeEye::KeyEvent(H, Key, true, bShift, false);
				Pl.Held.push_back(Player::Down{ Key, bShift, Pl.Clock + 0.3 + 3.7 * Pl.U(), Pl.Clock + 0.5 });
			}
			Pl.NextHold = Pl.Clock + 0.3 + 2.2 * Pl.U();
		}
		if (Pl.Clock >= Pl.NextWheel)
		{
			H.Wheel += (Pl.U() < 0.5 ? 1.0 : -1.0) * (1.0 + std::floor(3.0 * Pl.U()));
			Pl.NextWheel = Pl.Clock + 1.0 + 6.0 * Pl.U();
		}
		if (Pl.Clock >= Pl.NextPad)
		{
			H.SlideRight += 0.2 * (Pl.U() - 0.5);
			H.SlideAhead += 0.2 * (Pl.U() - 0.5);
			H.Pinch += Pl.U() < 0.5 ? 0.5 : -0.5;
			H.Twist += 20.0 * (Pl.U() - 0.5);
			Pl.NextPad = Pl.Clock + 10.0 + 20.0 * Pl.U();
		}
		if (Pl.Clock >= Pl.NextClick)
		{
			std::vector<const ShipOut*> Found, Hidden;
			for (const ShipOut& Ship : All)
			{
				if (Pickable(Ship))
				{
					Found.push_back(&Ship);
				}
				else if (!Ship.bShown && Ship.Doing != Fate::Gone)
				{
					Hidden.push_back(&Ship);
				}
			}
			const double Pick = Pl.U();
			Vec At{ Eye.X + 2000.0 * (Pl.U() - 0.5), Eye.Y + 2000.0 * (Pl.U() - 0.5), 0.0 };
			if (Pick < 0.3 && !Hidden.empty())
			{
				const ShipOut& Her = *Hidden[static_cast<size_t>(Pl.U() * static_cast<double>(Hidden.size())) % Hidden.size()];
				At = PlaceOf(Her);
				Pl.AimedHidden = Her.Key;
			}
			else if (Pick < 0.75 && !Found.empty())
			{
				At = PlaceOf(*Found[static_cast<size_t>(Pl.U() * static_cast<double>(Found.size())) % Found.size()]);
				At.Z += 3.0 * (Pl.U() - 0.5);
			}
			else if (Pick < 0.9)
			{
				// (the water between two of a division's ships in line: the division, C6)
				std::vector<int> Pieces;
				for (const ShipOut* Ship : Found)
				{
					if (Ship->Doing == Fate::Line && std::find(Pieces.begin(), Pieces.end(), Ship->Piece) == Pieces.end())
					{
						Pieces.push_back(Ship->Piece);
					}
				}
				std::sort(Pieces.begin(), Pieces.end());
				const double Which = Pl.U(), Gap = Pl.U();
				if (!Pieces.empty())
				{
					const std::vector<int> Members = MembersOf(All, Pieces[static_cast<size_t>(Which * static_cast<double>(Pieces.size())) % Pieces.size()], nullptr);
					if (Members.size() >= 2)
					{
						const size_t K = static_cast<size_t>(Gap * static_cast<double>(Members.size() - 1)) % (Members.size() - 1);
						At = (PlaceOf(*Named(All, Members[K])) + PlaceOf(*Named(All, Members[K + 1]))) * 0.5;
						At.Z = 0.0;
					}
				}
			}
			SeaFreeEye::ClickAt(H, SeaFreeEye::Ray{ Eye, At - Eye }, Pl.U() < 0.35);
			Pl.bPressed = true;
			Pl.NextClick = Pl.Clock + 2.0 + 6.0 * Pl.U();
		}
		if (Pl.Clock >= Pl.NextJ)
		{
			SeaFreeEye::KeyEvent(H, KeyName::J, true, false, false);
			SeaFreeEye::KeyEvent(H, KeyName::J, true, false, true); // (a repeat: never a second toggle)
			SeaFreeEye::KeyEvent(H, KeyName::J, false, false, false);
			Pl.bPressed = true;
			Pl.NextJ = Pl.Clock + 4.0 + 10.0 * Pl.U();
		}
		if (Pl.Clock >= Pl.NextTab)
		{
			const bool bShift = Pl.U() < 0.3;
			SeaFreeEye::KeyEvent(H, KeyName::Tab, true, bShift, false);
			if (Pl.U() < 0.2)
			{
				SeaFreeEye::KeyEvent(H, KeyName::Tab, true, bShift, true);
			}
			SeaFreeEye::KeyEvent(H, KeyName::Tab, false, bShift, false);
			Pl.bPressed = true;
			Pl.NextTab = Pl.Clock + 2.0 + 7.0 * Pl.U();
		}
		if (Pl.Clock >= Pl.NextC)
		{
			SeaFreeEye::KeyEvent(H, KeyName::C, true, false, false);
			SeaFreeEye::KeyEvent(H, KeyName::C, false, false, false);
			Pl.bPressed = Pl.bC = true;
			Pl.NextC = Pl.Clock + 30.0 + 40.0 * Pl.U();
		}
		if (Pl.Clock >= Pl.NextU && !bDiving)
		{
			const bool bShift = Pl.U() < 0.3;
			Pl.bUnder = SeaFreeEye::KeyEvent(H, KeyName::U, true, bShift, false) == SeaFreeEye::KeyTaken::Under;
			SeaFreeEye::KeyEvent(H, KeyName::U, true, bShift, true);
			SeaFreeEye::KeyEvent(H, KeyName::U, false, bShift, false);
			Pl.bUnderShift = bShift;
			Pl.bPressed = true;
			Pl.NextU = Pl.Clock + 40.0 + 60.0 * Pl.U();
		}
	}

	// ---- A recording played with the free eye --------------------------------------------------------------------------

	enum class Timing { Sixty, TwentyFour, Hitches, Jitter };

	struct RunSpec
	{
		std::string Name;
		Timing Frames = Timing::Sixty;
		double Pace = 1.0;
		/** 0 the still face, 1 the moving 12 m sea, 2 the real gale. */
		int Floor = 0;
		bool bDeck = false;
		double DeckBase = 0.0;
		/** Real seconds at most (0: to the battle's end, and half a minute past EndT + 12 where it stands still for good). */
		double RealMost = 0.0;
		/** A span of real seconds (from, for) where the battle stands still (paused). */
		double StillFrom = -1.0, StillFor = 0.0;
		bool bTour = false;
		bool bHiddenGone = false;
		double StartT = -1.0;
		uint64_t Seed = 1;
		/** The window's shape, and two changes of it (real seconds; Q8). */
		double Aspect = 16.0 / 9.0;
		double ShapeAt = -1.0;
	};

	/** Each check's failures (counted, with the first said) and what was seen, over every run. */
	struct Book
	{
		std::map<std::string, long> Bad;
		std::map<std::string, std::string> First;
		std::map<std::string, long> Seen;
		void Fail(const std::string& Id, const std::string& Words)
		{
			if (Bad[Id]++ == 0)
			{
				First[Id] = Words;
			}
		}
		void Count(const std::string& What, long N = 1) { Seen[What] += N; }
		void Most(const std::string& What, long N) { Seen[What] = std::max(Seen[What], N); }
	};

	struct RunOut
	{
		uint64_t Digest = 0;
		long Frames = 0, FreeFrames = 0;
		double LowestClear = 1e9, Highest = -1e9, LowestZ = 1e9;
		std::vector<std::string> Said;
		bool bTourOk = true;
		std::string TourWords;
	};

	RunOut Play(const Played& P, const RunSpec& R, Book& B)
	{
		RunOut Out;
		Digest G;
		SeaFreeEye::State St;
		FreeEye::Eye Free;
		Free.Set = SeaFreeEye::SeaSettings(R.Aspect);
		SeaFreeEye::SeaFloor Floor;
		MovingSea Sea;
		GaleFloor Gale;
		if (R.Floor == 1)
		{
			Floor.Drawn = &MovingSeaAt;
			Floor.Context = &Sea;
		}
		else if (R.Floor == 2)
		{
			FleetWaves::Asked A;
			A.WindU = FleetWaves::GaleCap;
			A.DownwindR = 0.6;
			A.Seed = 977;
			A.Cap = FleetWaves::GaleCap;
			A.Fetch = FleetWaves::GaleFetchOf(FleetWaves::GaleCap);
			Gale.S = FleetWaves::Make(A);
			Floor.Drawn = &GaleAt;
			Floor.Context = &Gale;
		}
		if (R.bTour)
		{
			std::string Error;
			if (!SeaFreeEye::ParseDrive(SeaFreeEye::TourText(), St.D, Error))
			{
				B.Fail("C15", "the tour does not read: " + Error);
				return Out;
			}
		}
		Player Pl(R.Seed);
		const double Ceil = CeilingOf(R.bDeck, R.DeckBase);
		std::vector<ShipOut> All, Fed;
		double RealT = 0.0, BattleT = R.StartT >= 0.0 ? R.StartT : P.BeginT, StillSince = -1.0, DiveUntil = -1.0;
		long Frame = 0;
		uint64_t Jit = Mix(R.Seed + 101u);
		Vec Shown{ 0.0, 0.0, 100.0 };
		bool bShownKnown = false;
		// (what the last frame gave, for the frame-to-frame checks)
		bool bWasFree = false, bWasFed = false, bWasLocked = false;
		double WasLens = 0.0;
		FreeEye::Input WasFed;
		int WasKey = -1, WasPiece = -1, WasId = -1;
		std::vector<int> WasMembers;
		int TourStepsSeen = 0;
		double TourWas = -1.0;
		const double TourTimes[] = { 12.5, 30.0, 40.0, 50.0 };
		while (true)
		{
			// The frame's real seconds.
			double Dt = 1.0 / 60.0;
			switch (R.Frames)
			{
			case Timing::Sixty: Dt = 1.0 / 60.0; break;
			case Timing::TwentyFour: Dt = 1.0 / 24.0; break;
			case Timing::Hitches: Dt = Frame % 997 == 500 ? 1.0 : Frame % 421 == 200 ? 0.3 : 1.0 / 60.0; break;
			case Timing::Jitter:
				Jit = Jit * 6364136223846793005ull + 1442695040888963407ull;
				Dt = 0.008 + 0.025 * static_cast<double>(Jit >> 11) * (1.0 / 9007199254740992.0);
				break;
			}
			++Frame;
			RealT += Dt;
			// The battle's clock: the scene's step held to 0.1 s, times the pace, standing still for good at EndT + 12.
			const bool bStill = R.StillFrom >= 0.0 && RealT >= R.StillFrom && RealT < R.StillFrom + R.StillFor;
			const double BattleWas = BattleT;
			if (!bStill)
			{
				BattleT = std::min(BattleT + std::min(Dt, 0.1) * R.Pace, P.EndT + 12.0);
			}
			if (BattleT >= P.EndT + 12.0 && StillSince < 0.0)
			{
				StillSince = RealT;
			}
			const bool bStands = BattleT == BattleWas;
			Sea.T = RealT;
			ShipsAt(P, std::min(BattleT, P.EndT), All, true);
			if (R.bHiddenGone)
			{
				ShipsAt(P, std::min(BattleT, P.EndT), Fed, false);
			}
			const std::vector<ShipOut>& Ships = R.bHiddenGone ? Fed : All;
			DirShot Dir = DirectorAt(P, BattleT, All);
			if (!bShownKnown)
			{
				Shown = Dir.Eye;
			}
			// His hands (or the drive's: the glue puts his aside), before the frame as the screen's key events come before
			// the scene's Tick: a U that goes under (PressU picks her boat at once) is his dive from this very frame.
			if (!R.bTour)
			{
				Act(Pl, St, Dt, Shown, All, RealT < DiveUntil);
				if (Pl.bUnder)
				{
					bool bDive = false;
					SeaFreeEye::UnderPressed(St, bDive);
					if (St.bOn)
					{
						B.Fail("C14", "U left him the free camera");
					}
					if (bDive)
					{
						DiveUntil = RealT + 6.0; // (he never presses U in a dive: Act)
						B.Count("dives");
					}
				}
			}
			const bool bDiving = RealT < DiveUntil;
			if (bDiving)
			{
				// (his own dive: the director's under-water view, by where the picture was)
				Dir.Eye = Vec{ Shown.X, Shown.Y, -10.0 };
				Dir.Look = Vec{ Shown.X + 50.0, Shown.Y + 50.0, -12.0 };
				Dir.Lens = 70.0;
				Dir.bUnder = true;
			}
			// The window's shape changes (Q8: the lens refitted).
			double Aspect = R.Aspect;
			if (R.ShapeAt >= 0.0)
			{
				Aspect = RealT < R.ShapeAt ? R.Aspect : RealT < 2.0 * R.ShapeAt ? 1.6 : 1.54;
			}
			// The real gale's frame: its rings and Near built for this frame's clock by where the eye stood, before the eye.
			if (R.Floor == 2)
			{
				const double Eye3[3] = { Shown.X, Shown.Y, Shown.Z }, Look3[3] = { Shown.X + 100.0, Shown.Y + 100.0, 0.0 };
				Gale.D = FleetWaves::DrawnFor(Gale.S, Eye3, Look3, 60.0, nullptr, RealT, Gale.bD ? &Gale.D : nullptr, !Gale.bD, FleetWaves::RingsFor(Gale.S, 96), 96);
				Gale.bD = true;
				FleetWaves::BuildNear(Gale.S, Gale.D, Gale.N);
			}
			SeaFreeEye::Scene In;
			In.Ships = &Ships;
			In.MySide = P.Mine;
			In.RealDt = Dt;
			In.Aspect = Aspect;
			In.DirEye = Dir.Eye;
			In.DirLook = Dir.Look;
			In.DirLens = Dir.Lens;
			In.bHandDive = bDiving;
			In.Floor = &Floor;
			In.BoardSize = P.Size;
			In.CeilingNow = SeaFreeEye::Ceiling(R.bDeck, R.DeckBase);
			In.bDriveMayStart = true;
			const int KeyBefore = St.L.Key;
			const bool bWasOn = St.bOn;
			const double DriveWas = St.D.T;
			const SeaFreeEye::Shot O = SeaFreeEye::Step(St, Free, In);
			const FreeEye::Input& F = St.Fed;
			Out.Frames++;

			// The digest: everything given, bit for bit.
			G.I(O.bFree);
			G.V(O.Eye);
			G.V(O.Look);
			G.D(O.Lens);
			G.I(O.bCut);
			G.I(O.bGiveBack);
			G.I(O.bTaken);
			G.I(O.bLockChanged);
			G.I(O.Hero);
			G.I(O.Locked);
			G.I(St.PickKey);
			G.I(St.PickPiece);
			G.I(St.bFed);
			if (St.bFed)
			{
				G.I(F.bLock);
				G.I(F.AnchorId);
				G.V(F.Anchor);
				G.I(F.bAnchorVel);
				G.V(F.AnchorVel);
				G.D(F.AnchorClear);
				G.D(F.AnchorHeading);
				G.D(F.PanRight);
				G.D(F.PanAhead);
				G.D(F.Rise);
				G.D(F.Turn);
				G.D(F.Tilt);
				G.D(F.Zoom);
			}
			if (Frame % 30 == 0)
			{
				for (const int Key : SeaFreeEye::TabOrder(Ships, P.Mine))
				{
					G.I(Key);
				}
			}
			for (const std::string& Line : SeaFreeEye::TakeSaid(St))
			{
				G.S(Line);
				Out.Said.push_back(Line);
			}

			// The ships he may reach (C3): the lock, the pick, the division's members, all found and afloat.
			if (St.L.Key >= 0)
			{
				const ShipOut* Her = Named(Ships, St.L.Key);
				if (!Her || !Pickable(*Her))
				{
					B.Fail("C3", P.Name + ": a lock kept on ship " + std::to_string(St.L.Key) + " not found or not afloat at " + Num(BattleT, 2) + " s");
				}
			}
			if (St.bFed && F.bLock)
			{
				if (St.L.Key >= 0)
				{
					const ShipOut* Her = Named(Ships, F.AnchorId);
					if (!Her || !Pickable(*Her))
					{
						B.Fail("C3", P.Name + ": a lock sent on ship " + std::to_string(F.AnchorId) + " not found or not afloat");
					}
				}
				else
				{
					Vec Middle;
					const std::vector<int> Members = MembersOf(Ships, St.L.Piece, &Middle);
					if (Members.empty() || Members != St.L.Members)
					{
						B.Fail("C6", P.Name + ": a division's members not its found ships in line");
					}
					else if ((F.Anchor - Middle).Length() > 1e-6)
					{
						B.Fail("C6", P.Name + ": a division's middle not its found members' (" + Num((F.Anchor - Middle).Length(), 3) + " m off)");
					}
					if (F.AnchorId < SeaFreeEye::DivisionIds)
					{
						B.Fail("C6", P.Name + ": a division's id among the ships' keys");
					}
				}
			}
			if (St.PickKey >= 0)
			{
				const ShipOut* Her = Named(Ships, St.PickKey);
				if (!Her || !Pickable(*Her))
				{
					B.Fail("C3", P.Name + ": a pick kept on ship " + std::to_string(St.PickKey) + " not found or not afloat");
				}
			}
			if (St.PickPiece >= 0 && MembersOf(Ships, St.PickPiece, nullptr).empty())
			{
				B.Fail("C3", P.Name + ": a division picked with none of it found in line");
			}
			if (Pl.AimedHidden >= 0)
			{
				B.Count("clicks straight at hidden boats");
				if (St.PickKey == Pl.AimedHidden || F.AnchorId == Pl.AimedHidden)
				{
					B.Fail("C3", P.Name + ": a click straight at hidden boat " + std::to_string(Pl.AimedHidden) + " reached her");
				}
			}
			if (Frame % 120 == 0)
			{
				// (every hidden boat, a ray from the eye straight through her: never her)
				for (const ShipOut& Ship : All)
				{
					if (!Ship.bShown)
					{
						B.Count("rays through hidden boats");
						const Vec Eye = O.bFree ? O.Eye : Dir.Eye;
						if (SeaFreeEye::PickAlongRay(All, SeaFreeEye::Ray{ Eye, PlaceOf(Ship) - Eye }) == Ship.Key)
						{
							B.Fail("C3", P.Name + ": a ray through hidden boat " + std::to_string(Ship.Key) + " picked her");
						}
					}
				}
				// (C5: the Tab order against its recount)
				if (SeaFreeEye::TabOrder(Ships, P.Mine) != Recount(Ships, P.Mine))
				{
					B.Fail("C5", P.Name + ": the Tab order differs from its recount at " + Num(BattleT, 2) + " s");
				}
				B.Count("Tab orders recounted");
			}

			// Let go (C4): she was locked, and is now lost, sinking, gone or not there: no lock sent this frame, no cut.
			if (WasKey >= 0 && WasKey == KeyBefore)
			{
				const ShipOut* Her = Named(Ships, WasKey);
				if (!Her || !Pickable(*Her))
				{
					B.Count("let goes on a ship lost, sinking or gone");
					if (St.bFed && F.bLock && F.AnchorId == WasKey)
					{
						B.Fail("C4", P.Name + ": ship " + std::to_string(WasKey) + " lost and still sent");
					}
					if (O.bCut && !Pl.bPressed && !O.bTaken)
					{
						B.Fail("C4", P.Name + ": a let go cut the picture");
					}
				}
				else if (St.L.Key != WasKey && !Pl.bPressed && !R.bTour)
				{
					B.Fail("C7", P.Name + ": a lock on a ship still afloat and found let go with nothing pressed (pace " + Num(R.Pace, 2) + ")");
				}
			}

			// A division's id (C6): renewed whenever its members change, never otherwise.
			if (St.bFed && F.bLock && St.L.Piece >= 0 && bWasFed && WasFed.bLock && WasPiece == St.L.Piece)
			{
				const bool bChanged = St.L.Members != WasMembers;
				if (bChanged != (F.AnchorId != WasId))
				{
					B.Fail("C6", P.Name + ": a division's id " + std::string(bChanged ? "kept through a change of its members" : "changed with its members the same"));
				}
				B.Count(bChanged ? "division member changes" : "division frames");
			}

			// Real seconds (C7): the velocity as the scene moved her over this frame's real seconds; none on the first.
			if (St.bFed && F.bLock)
			{
				const bool bFirst = !(bWasFed && WasFed.bLock && WasFed.AnchorId == F.AnchorId) || O.bTaken;
				if (bFirst && F.bAnchorVel)
				{
					B.Fail("C7", P.Name + ": a lock's first frame sent a velocity");
				}
				if (!bFirst && F.bAnchorVel)
				{
					const Vec Want = (F.Anchor - WasFed.Anchor) * (1.0 / Dt);
					if ((F.AnchorVel - Want).Length() > 1e-6 * std::max(1.0, Want.Length()))
					{
						B.Fail("C7", P.Name + ": a lock's velocity " + Num(F.AnchorVel.Length(), 3) + " m/s, moved " + Num(Want.Length(), 3) + " m/s over the real seconds");
					}
					B.Count("velocities measured");
				}
			}
			if (bStands && O.bFree && St.bFed && (F.PanAhead != 0.0 || F.PanRight != 0.0))
			{
				B.Count("frames flown while the battle stands still");
			}

			// The eye (C1, C2, the box), the take (C13), the lens (C12), the dive (C14).
			if (O.bFree)
			{
				Out.FreeFrames++;
				const double Drawn = R.Floor == 1 ? MovingSeaAt(&Sea, O.Eye.X, O.Eye.Y) : R.Floor == 2 ? FleetWaves::SampleNear(Gale.N, O.Eye.X, O.Eye.Y) : 0.0;
				const double Clear = O.Eye.Z - std::max(Drawn, 0.0);
				Out.LowestClear = std::min(Out.LowestClear, Clear);
				Out.LowestZ = std::min(Out.LowestZ, O.Eye.Z);
				Out.Highest = std::max(Out.Highest, O.Eye.Z);
				if (Clear < HardClear - 1e-6 || O.Eye.Z < HardClear - 1e-6)
				{
					B.Fail("C1", P.Name + ": the eye " + Num(Clear, 3) + " m over the drawn sea (" + Num(O.Eye.Z, 3) + " m over 0) at " + Num(RealT, 2) + " s");
				}
				if (R.Floor == 2)
				{
					const double Exact = FleetWaves::DrawnSurfaceZ(Gale.S, Gale.D, O.Eye.X, O.Eye.Y);
					B.Most("gale: frames checked against the exact drawn surface", ++B.Seen["gale frames"]);
					if (O.Eye.Z - Exact < 0.45)
					{
						B.Fail("C1", P.Name + ": the eye " + Num(O.Eye.Z - Exact, 3) + " m over the gale's exact drawn surface");
					}
					B.Most("gale: crest under the eye, cm", static_cast<long>(std::lround(100.0 * std::max(Exact, 0.0))));
				}
				if (O.Eye.Z > Ceil + 0.01)
				{
					B.Fail("C2", P.Name + ": the eye at " + Num(O.Eye.Z, 2) + " m over the ceiling " + Num(Ceil, 0));
				}
				// (the pivot's box, 1i: X and Y the board and 3 km, Z -20 m to the ceiling; the core's hard caps go the floor
				// first, then the ceiling, the anchor's clearance and the box last (FreeEye.cpp), and the floor's lift raises
				// the pivot with the eye: looking up from low on a long boom, the floor holds the eye and lifts the pivot over
				// the top. So the pivot less the floor's lift is held to the box's top, exactly as the core keeps it)
				const Vec Pv = Free.Pivot();
				const bool bFloorHeld = Pv.Z - Free.Lift() <= Ceil + 0.01;
				if (Pv.Z > Ceil + 0.01 && bFloorHeld)
				{
					B.Count("frames with the pivot over the box's top under an eye the floor holds");
					B.Most("the pivot over the box's top under an eye the floor holds, cm", static_cast<long>(std::lround(100.0 * (Pv.Z - Ceil))));
				}
				if (Pv.Z < -20.0 - 0.01 || (Pv.Z > Ceil + 0.01 && !bFloorHeld) || Pv.X < -3000.01 || Pv.Y < -3000.01 || Pv.X > P.Size + 3000.01 || Pv.Y > P.Size + 3000.01)
				{
					B.Fail("C2", P.Name + ", " + R.Name + ": the pivot out of its box at (" + Num(Pv.X, 1) + ", " + Num(Pv.Y, 1) + ", " + Num(Pv.Z, 1) + "), the eye at (" + Num(O.Eye.X, 1) + ", " +
						Num(O.Eye.Y, 1) + ", " + Num(O.Eye.Z, 1) + ") at " + Num(RealT, 2) + " s");
				}
				if (O.bTaken)
				{
					B.Count("takes");
					if (std::abs(O.Lens - Dir.Lens) > 1e-9)
					{
						B.Fail("C12", P.Name + ": a take changed the picture's width");
					}
					if (!O.bCut)
					{
						const double Far = (Dir.Look - Dir.Eye).Length();
						if ((O.Eye - Dir.Eye).Length() > 0.01 || Angle(O.Look - O.Eye, Dir.Look - Dir.Eye) > 1e-6 ||
							(Far >= 10.0 && Far <= 4000.0 && (O.Look - Dir.Look).Length() > 0.01))
						{
							B.Fail("C13", P.Name + ", " + R.Name + ": a take " + Num((O.Eye - Dir.Eye).Length(), 3) + " m from the director's eye (" + Num(Dir.Eye.X, 1) + ", " + Num(Dir.Eye.Y, 1) + ", " +
								Num(Dir.Eye.Z, 1) + ") at " + Num(RealT, 2) + " s, " + (St.L.On() ? "locked on " + std::to_string(St.L.Id) : std::string("no lock")));
						}
					}
					else
					{
						B.Count("takes lifted out of the water or let down under the ceiling (a cut)");
						if (Dir.Eye.Z < 0.0 && O.Eye.Z < HardClear)
						{
							B.Fail("C14", P.Name + ": a take under the water not lifted clear");
						}
						// (a shot over the free ceiling, under a low deck, is let down to it still looking at what the
						// director looked at, the pivot on his line of sight and so in its box)
						if (Dir.Eye.Z > Ceil + 0.01)
						{
							B.Count("takes let down from over the ceiling");
							if (std::abs(O.Eye.Z - Ceil) > 0.01 || Angle(O.Look - O.Eye, Dir.Look - O.Eye) > 1e-6)
							{
								B.Fail("C13", P.Name + ", " + R.Name + ": a take from over the ceiling not let down to it looking where the director looked");
							}
						}
					}
					if (Dir.bUnder && !O.bCut)
					{
						B.Fail("C14", P.Name + ": a take from an eye under the water was not a cut");
					}
					if (bWasOn)
					{
						B.Fail("C19", P.Name + ", " + R.Name + ": a take while he had the camera, at " + Num(RealT, 2) + " s");
					}
				}
				else if (bWasFree && !O.bCut)
				{
					const double Most = LensMostPerSecond * std::min(Dt, 0.25) + 1e-9;
					if (std::abs(O.Lens - WasLens) > Most)
					{
						B.Fail("C12", P.Name + ": the lens moved " + Num(std::abs(O.Lens - WasLens), 4) + " degrees in " + Num(Dt, 4) + " s");
					}
				}
				if (O.bCut)
				{
					B.Count("cuts of the free eye");
				}
			}
			if (bDiving && O.bFree)
			{
				B.Fail("C14", P.Name + ": the free eye had the camera through his dive");
			}
			if (O.bGiveBack)
			{
				B.Count("give backs");
				if (St.L.On() || St.PickKey >= 0 || St.PickPiece >= 0)
				{
					B.Fail("C19", P.Name + ": something kept through the give back");
				}
			}
			if (Pl.bC && (St.L.On() || St.bOn || O.bFree))
			{
				B.Fail("C19", P.Name + ": C left the lock or the camera (its own frame the director's)");
			}

			// The tour (C15): its Tabs and its J, each at its time.
			if (R.bTour && St.D.bStarted)
			{
				for (const double At : TourTimes)
				{
					if (DriveWas < At && St.D.T >= At && TourWas < At)
					{
						TourWas = At;
						++TourStepsSeen;
						const std::vector<int> Order = Recount(Ships, P.Mine);
						const bool bJ = At == 50.0;
						int Want = -1;
						if (!bJ)
						{
							const ShipOut* Before = Named(Ships, KeyBefore);
							Want = SeaFreeEye::TabStep(Order, KeyBefore >= 0 && Before && Pickable(*Before) ? KeyBefore : -1, 1);
						}
						const bool bRight = bJ ? (KeyBefore < 0 || St.L.Key < 0) : St.L.Key == Want;
						Out.TourWords += (Out.TourWords.empty() ? "" : ", ") + Num(At, 1) + " s " + (bJ ? "J" : "Tab") + " " +
							(St.L.Key >= 0 ? "on ship " + std::to_string(St.L.Key) : std::string("let go"));
						if (!bRight)
						{
							Out.bTourOk = false;
							B.Fail("C15", P.Name + ": the tour's " + (bJ ? std::string("J") : std::string("Tab")) + " at " + Num(At, 1) + " s on " + std::to_string(St.L.Key) + ", not " + std::to_string(Want));
						}
						if (At == 12.5 && Want >= 0 && Named(Ships, Want)->Side != P.Mine && !Order.empty() && Named(Ships, Order.front())->Side == P.Mine)
						{
							B.Fail("C15", P.Name + ": the tour's first Tab not on his own side");
						}
					}
				}
			}

			// What this frame leaves for the next.
			bWasFree = O.bFree;
			WasLens = O.Lens;
			bWasFed = St.bFed;
			WasFed = F;
			WasKey = St.L.Key;
			WasPiece = St.L.Piece;
			WasId = St.L.On() ? St.L.Id : -1;
			WasMembers = St.L.Members;
			bWasLocked = St.L.On();
			Shown = O.bFree ? O.Eye : Dir.Eye;
			bShownKnown = true;
			// The end of the run.
			if (R.bTour && St.D.bDone && RealT > 100.0)
			{
				break;
			}
			if (R.RealMost > 0.0 ? RealT >= R.RealMost : (StillSince >= 0.0 && RealT >= StillSince + 30.0))
			{
				break;
			}
		}
		(void)bWasLocked;
		if (R.bTour)
		{
			const SeaFreeEye::Drive& D = St.D;
			int StepsRun = 0;
			bool bTimes = true;
			for (size_t K = 0; K < D.Starts.size(); ++K)
			{
				StepsRun += D.StepFrames[K] > 0 ? 1 : 0;
				const double Planned = K + 1 < D.Starts.size() ? D.Starts[K + 1] - D.Starts[K] : 0.0;
				bTimes = bTimes && (K + 1 == D.Starts.size() || std::abs(D.StepSeconds[K] - Planned) <= 2.5 / 60.0);
			}
			const bool bLines = !Out.Said.empty() && Out.Said.back().find("the drive is over") != std::string::npos;
			// (the core's soft ceiling holds a steep far view short of it and lets it creep up: after the climb's 28 s the
			// eye stands 1 to 3 m under it, so "reaches the ceiling" is held as within 5 m of it, a third of a per cent, and
			// never over it)
			Out.bTourOk = Out.bTourOk && D.bDone && StepsRun == static_cast<int>(D.Starts.size()) && bTimes && TourStepsSeen == 4 && bLines &&
				D.Highest >= Ceil - 5.0 && D.Highest <= Ceil + 0.01 && !St.bOn;
			Out.TourWords = std::to_string(StepsRun) + " of " + std::to_string(D.Starts.size()) + " steps run" + (bTimes ? " at their times" : " (NOT at their times)") +
				", the eye up to " + Num(D.Highest, 1) + " m (ceiling " + Num(Ceil, 0) + "), " + (St.bOn ? "the camera NOT given back" : "the camera given back") + "; " +
				Out.TourWords;
		}
		Out.Digest = G.H;
		return Out;
	}

	// ---- The scene's text ------------------------------------------------------------------------------------------

	std::string FunctionOf(const std::string& Cpp, const std::string& Head)
	{
		const size_t At = Cpp.find("\n" + Head);
		if (At == std::string::npos)
		{
			return std::string();
		}
		const size_t End = Cpp.find("\n}\n", At + 1);
		return End == std::string::npos ? std::string() : Cpp.substr(At + 1, End + 3 - (At + 1));
	}

	/** AimEye before its cut line (the free block left out), the cut line, and after it. */
	bool AimEyePieces(const std::string& Cpp, std::string& Before, std::string& Cut, std::string& After, std::string& Block)
	{
		std::string T = FunctionOf(Cpp, "void ASeaBattle::AimEye(");
		if (T.empty())
		{
			return false;
		}
		const size_t Begins = T.find("// SeaFreeEye block begins");
		Block.clear();
		if (Begins != std::string::npos)
		{
			const size_t From = T.rfind('\n', Begins) + 1;
			const size_t Ends = T.find("// SeaFreeEye block ends", Begins);
			if (Ends == std::string::npos)
			{
				return false;
			}
			const size_t To = T.find('\n', Ends) + 1;
			Block = T.substr(From, To - From);
			T = T.substr(0, From) + T.substr(To);
		}
		const size_t Moved = T.find("const bool bMoved");
		const size_t C = Moved == std::string::npos ? std::string::npos : T.find("\n\tif (", Moved);
		if (C == std::string::npos)
		{
			return false;
		}
		const size_t CutEnd = T.find('\n', C + 1) + 1;
		Before = T.substr(0, C + 1);
		Cut = T.substr(C + 1, CutEnd - (C + 1));
		After = T.substr(CutEnd);
		return true;
	}

	size_t CountOf(const std::string& Text, const std::string& Word)
	{
		size_t N = 0;
		for (size_t At = Text.find(Word); At != std::string::npos; At = Text.find(Word, At + 1))
		{
			++N;
		}
		return N;
	}

	bool Has(const std::string& Text, const std::string& Piece)
	{
		return Text.find(Piece) != std::string::npos;
	}

	/** The name as a whole name of the code (UnderWindow is not UnderWindows). */
	bool HasName(const std::string& Text, const std::string& Name)
	{
		auto NameChar = [](char C) { return std::isalnum(static_cast<unsigned char>(C)) != 0 || C == '_'; };
		for (size_t At = Text.find(Name); At != std::string::npos; At = Text.find(Name, At + 1))
		{
			const bool bFrontClear = At == 0 || !NameChar(Text[At - 1]);
			const bool bBackClear = At + Name.size() >= Text.size() || !NameChar(Text[At + Name.size()]);
			if (bFrontClear && bBackClear)
			{
				return true;
			}
		}
		return false;
	}

	void SceneText(const std::string& Src, const std::string& ShaPath)
	{
		std::printf("The scene's text (Battle/SeaBattle.cpp, the switches, the glue)\n");
		const std::string Cpp = ReadAll(Src + "/Battle/SeaBattle.cpp");
		const std::string Switches = ReadAll(Src + "/Battle/SeaLookSwitches.h");
		const std::string GlueH = ReadAll(Src + "/Battle/SeaFreeEyeGlue.h"), GlueCpp = ReadAll(Src + "/Battle/SeaFreeEyeGlue.cpp");
		const std::string Pins = ReadAll(ShaPath);
		std::map<std::string, std::string> Pinned;
		{
			std::istringstream Lines(Pins);
			std::string Line;
			while (std::getline(Lines, Line))
			{
				if (Line.size() > 65 && Line[0] != '#')
				{
					Pinned[Line.substr(65)] = Line.substr(0, 64);
				}
			}
		}
		// C9: the director's text.
		const std::string Plan = FunctionOf(Cpp, "void ASeaBattle::PlanShots("), Shot = FunctionOf(Cpp, "bool ASeaBattle::ShotEye(");
		std::string Before, Cut, After, Block;
		const bool bPieces = AimEyePieces(Cpp, Before, Cut, After, Block);
		const std::string PlanSha = Sha256(Plan), ShotSha = Sha256(Shot), BeforeSha = Sha256(Before), AfterSha = Sha256(After);
		Check("C9", "PlanShots and ShotEye word for word as director.sha pins them (found by their signatures)",
			!Plan.empty() && !Shot.empty() && PlanSha == Pinned["PlanShots"] && ShotSha == Pinned["ShotEye"], PlanSha.substr(0, 16) + ", " + ShotSha.substr(0, 16));
		Check("C9", "AimEye's director part word for word: its text before the cut line (the free block left out) and after it, as pinned on the base",
			bPieces && BeforeSha == Pinned["AimEye before its cut line"] && AfterSha == Pinned["AimEye after its cut line"], BeforeSha.substr(0, 16) + ", " + AfterSha.substr(0, 16));
		// (Free anywhere in a name; the members the free path writes as whole names)
		std::string Read = Has(Plan, "Free") || Has(Shot, "Free") ? "Free" : "";
		const char* Writes[] = { "EyeAt", "EyeLens", "EyeFacing", "TicksSinceCut", "ShotNow", "bEyeCut", "FunnelsShot", "UnderWindow", "FunnelsChosenAt", "WrecksFarAt" };
		for (const char* Word : Writes)
		{
			if (HasName(Plan, Word) || HasName(Shot, Word))
			{
				Read += std::string(Read.empty() ? "" : ", ") + Word;
			}
		}
		Check("C9", "PlanShots and ShotEye read nothing the free path writes (no Free word in either) and PlanShots(); is called once", Read.empty() && CountOf(Cpp, "PlanShots();") == 1,
			Read.empty() ? std::to_string(CountOf(Cpp, "PlanShots();")) + " call" : Read);
		Check("C9", "no name of the glue holds Reel or RideStrip (reel_test scans AimEye's body for them)",
			!Has(GlueH, "Reel") && !Has(GlueCpp, "Reel") && !Has(GlueH, "RideStrip") && !Has(GlueCpp, "RideStrip") && !Has(Block, "Reel"), "none");
		// C8: the screen's keys. LOCK ON is J in all three scenes (the UI director, 27 Nov 06:30Z): L is not the sea screen's,
		// so with the switch on it goes on to the campaign (Forces) as with it off.
		{
			const std::string Keys = FunctionOf(ReadAll(Src + "/Battle/SeaBattleScreen.cpp"), "bool SSeaBattleScreen::FreeKeyOf(");
			std::set<std::string> Named;
			for (size_t At = Keys.find("EKeys::"); At != std::string::npos; At = Keys.find("EKeys::", At + 1))
			{
				size_t End = At + 7;
				while (End < Keys.size() && (std::isalnum(static_cast<unsigned char>(Keys[End])) != 0 || Keys[End] == '_'))
				{
					++End;
				}
				Named.insert(Keys.substr(At + 7, End - (At + 7)));
			}
			const std::set<std::string> Want = { "W", "A", "S", "D", "Up", "Down", "Left", "Right", "Q", "E", "J", "C", "Tab", "U" };
			std::string Words;
			for (const std::string& Name : Named)
			{
				Words += (Words.empty() ? "" : " ") + Name;
			}
			SeaFreeEye::Drive WithL, WithJ;
			std::string LError, JError;
			const bool bLRead = SeaFreeEye::ParseDrive("50 press L\n", WithL, LError);
			const bool bJRead = SeaFreeEye::ParseDrive("50 press J\n", WithJ, JError);
			Check("C8", "L is not the sea screen's: its key function names exactly W A S D, the four arrows, Q, E, J, C, Tab and U (J locks and lets go), so L goes on to the campaign (Forces) as with the switch off; a drive's press L refused, press J read",
				!Keys.empty() && Named == Want && Has(Keys, "Key == EKeys::J ? KeyName::J :") && !bLRead && bJRead,
				(Keys.empty() ? std::string("no FreeKeyOf") : Words) + (bLRead ? ", press L READ" : "") + (bJRead ? "" : ", press J refused: " + JError));
		}
		// C17: the one changed line, and the free block's under-window rule.
		Check("C17", "AimEye's cut is CutNow, its terms today's, in today's order",
			Cut == "\tif (SeaFreeEye::CutNow(bFreeNow, bEyeCut, bFreeCut, Want != ShotNow, bMoved, (From.Z < 0.0) != (EyeAt.Z < 0.0), UnderWindow != WasWindow))\n", Cut.substr(0, Cut.size() ? Cut.size() - 1 : 0));
		Check("C17", "the free block: the free eye's frame with his dive the director's, and no under window of the director's while free",
			Has(Block, "FreeAim(Standing == FSeaUnder::ByHandWindow, From, To, Lens, bFreeNow, bFreeCut);") && Has(Block, "\t\tif (bFreeNow)\n\t\t{\n\t\t\tUnderWindow = INDEX_NONE;\n\t\t}\n") &&
				Has(Block, "\tbFreeNow = false;\n\tif (bFreeEye)\n"),
			std::to_string(CountOf(Block, "\n")) + " lines");
		// C18: the take in OpenOn, the rows built exactly when it is there, the swell's tripwire.
		const std::string Open = FunctionOf(Cpp, "bool ASeaBattle::OpenOn(");
		const size_t ReadAt = Open.find("SeaLook = SeaLookSwitches::Read("), TakeAt = Open.find("SeaFreeEye::Take(SeaLook, !bMonthScene && ShotFolder.IsEmpty())"),
					 PlanAt = Open.find("PlanShots();");
		const bool bTakeThere = ReadAt != std::string::npos && TakeAt != std::string::npos && PlanAt != std::string::npos && ReadAt < TakeAt && TakeAt < PlanAt;
		Check("C18", "OpenOn takes the switches with SeaFreeEye::Take(SeaLook, !bMonthScene && ShotFolder.IsEmpty()) after the switches' Read and before PlanShots();", bTakeThere,
			bTakeThere ? "in order" : "not there in order");
		int Rows = 0, Built = 0;
		bool bSwellBuilt = false;
		for (const SeaLookSwitches::Switch& One : SeaLookSwitches::All)
		{
			const std::string Name = One.Name;
			if (Name.rfind("TheaterSeaFreeEye", 0) == 0)
			{
				++Rows;
				Built += One.bBuilt ? 1 : 0;
			}
			bSwellBuilt = bSwellBuilt || ((Name == "TheaterSeaSwell=" || Name == "TheaterSeaGale") && One.bBuilt);
		}
		const bool bRowsRead = Has(Switches, "{ \"TheaterSeaFreeEye=\", \"0\", \"own\", \"FE1\", true }") && Has(Switches, "{ \"TheaterSeaFreeEyeRide=\", \"0\", \"with FreeEye\", \"FE1\", true }") &&
			Has(Switches, "{ \"TheaterSeaFreeEyeDrive=\", \"off\", \"with FreeEye\", \"FE1\", true }");
		Check("C18", "the three rows built exactly when OpenOn's take is there (and the header's text says so)", Rows == 3 && (Built == 3) == bTakeThere && (Built == 0 || Built == 3) && (bRowsRead == bTakeThere),
			std::to_string(Built) + " of " + std::to_string(Rows) + " built");
		Check("C18", "the swell's tripwire: -TheaterSeaSwell= or -TheaterSeaGale built while the free floor is still the flat face fails",
			!bSwellBuilt || Has(Cpp, "FreeFloor.Drawn ="), bSwellBuilt ? "the swell built, the free floor handed the drawn sea" : "neither built: the flat face is the drawn sea");
		// C7, C14, C20: the scene moves values, the rules are the glue's.
		const std::string Tick = FunctionOf(Cpp, "void ASeaBattle::Tick("), Aim = FunctionOf(Cpp, "void ASeaBattle::FreeAim("), Key = FunctionOf(Cpp, "bool ASeaBattle::FreeKey(");
		const size_t FreeDtAt = Tick.find("\tFreeDt = DeltaSeconds;"), ClampAt = Tick.find("const double Dt = FMath::Clamp(static_cast<double>(DeltaSeconds), 0.0, 0.1);");
		Check("C7", "the free eye's seconds are the frame's real seconds, unclamped and never times the pace (Tick, FreeAim)",
			FreeDtAt != std::string::npos && ClampAt != std::string::npos && FreeDtAt < ClampAt && CountOf(Cpp, "FreeDt =") == 1 && Has(Aim, "\tIn.RealDt = FreeDt;\n"), "FreeDt = DeltaSeconds");
		Check("C14", "U: the under-water view's own key read only when the free eye is off; with it on, U gives the camera back first, then goes to UnderKey",
			Has(Tick, "!bMonthScene && ShotFolder.IsEmpty() && !bFreeEye);") && Has(Key, "SeaFreeEye::UnderPressed(FreeState, bDive);") && Has(Key, "\tif (bDive)\n\t{\n\t\tUnderKey(bShift);"), "as ruled");
		const std::string Trails = FunctionOf(Cpp, "void ASeaBattle::RunTrails("), Wrecks = FunctionOf(Cpp, "void ASeaBattle::DrawWrecks(");
		Check("C20", "free, the funnels are chosen by the glue's rule (its cut, its hero, 1 km, 4 real seconds), never on the director's shot; off, today's",
			Has(Trails, "\tconst bool bFunnelsFree = bFunnels && bFreeNow;\n\tif (bFunnelsFree ? SeaFreeEye::FunnelsDue(FunnelsFree, FreeRealNow, SeaFreeEye::Vec{ BoardOf(EyeAt).X, BoardOf(EyeAt).Y, BoardOf(EyeAt).Z }, TicksSinceCut == 0, FreeHero)\n"
								"\t                 : (bFunnels && (Now - FunnelsChosenAt >= 4.0 || ShotNow != FunnelsShot)))\n") &&
				Has(Trails, "\t\tif (bFunnelsFree)\n\t\t{\n\t\t\tHero = FreeHero;\n\t\t}\n\t\telse if (Shots.IsValidIndex(ShotNow))\n"),
			"RunTrails");
		Check("C20", "free, the far wrecks are laid by the glue's rule (its cut, half LiveReach moved, half a real second); off, today's",
			Has(Wrecks, "\tconst bool bFarDue = bFreeNow ? SeaFreeEye::FarWreckDue(WrecksFree, FreeRealNow, SeaFreeEye::Vec{ EyeBoard.X, EyeBoard.Y, EyeBoard.Z }, TicksSinceCut == 0, 0.5 * LiveReach)\n"
								"\t                              : (TicksSinceCut == 0 || Now - WrecksFarAt >= WreckFarEvery || Now < WrecksFarAt);\n"),
			"DrawWrecks");
		// C14 again: U's dive aimed from the director's eye of the frame shown when it gave the free camera back, never from
		// where the free eye was flown (Q9: no probe); FreeAim keeps that eye before it puts the free eye's in From.
		const std::string UKey = FunctionOf(Cpp, "void ASeaBattle::UnderKey(");
		const size_t DirAt = Aim.find("\tFreeDirAt = From;"), FreeAt = Aim.find("\tFrom = World(");
		Check("C14", "U that gave the free camera back dives from the director's eye of that frame (kept in FreeAim before the free eye's goes into From), never from the free eye's; off, from EyeAt as today",
			Has(UKey, "Under->PressU(bShift, State->Show, State->BattleT, bFreeNow ? FreeDirAt : EyeAt);") && DirAt != std::string::npos && FreeAt != std::string::npos && DirAt < FreeAt &&
				CountOf(Cpp, "FreeDirAt =") == 1,
			DirAt != std::string::npos && DirAt < FreeAt ? "the director's eye" : "NOT the director's eye");
		// C8: the screen's routing (critic mark C10). Its keys go through the glue's ScreenKey (played below), its answer the screen's: no
		// other key Handled, a release Handled only as ScreenKey says; the wheel's notches handed on with their sign.
		{
			const std::string Screen = ReadAll(Src + "/Battle/SeaBattleScreen.cpp");
			const std::string Down = FunctionOf(Screen, "FReply SSeaBattleScreen::OnKeyDown("), Up = FunctionOf(Screen, "FReply SSeaBattleScreen::OnKeyUp(");
			const std::string Keys = FunctionOf(Screen, "bool SSeaBattleScreen::FreeKeyOf("), Wheel = FunctionOf(Screen, "FReply SSeaBattleScreen::OnMouseWheel(");
			const std::string Lost = FunctionOf(Screen, "void SSeaBattleScreen::OnFocusLost(");
			const std::string SceneKey = FunctionOf(Cpp, "bool ASeaBattle::FreeKey("), SceneWheel = FunctionOf(Cpp, "void ASeaBattle::FreeWheel("),
							  SceneLost = FunctionOf(Cpp, "void ASeaBattle::FreeLetGoKeys(");
			// (OnKeyDown's free part is its last line; before it only the month scene's, behind IsMonthScene)
			const size_t Month = Down.find("\tif (Battle.IsValid() && Battle->IsMonthScene())\n\t{\n"), MonthEnd = Down.find("\n\t}\n", Month == std::string::npos ? 0 : Month);
			const std::string FreePart = MonthEnd == std::string::npos ? Down : Down.substr(MonthEnd + 4);
			std::string Wrong;
			auto Need = [&Wrong](bool bOk, const char* What) { Wrong += bOk ? "" : std::string(Wrong.empty() ? "" : ", ") + What; };
			Need(FreePart == "\t// The sea free eye: the camera's keys to the scene (Handled), every other key on to the campaign (Unhandled).\n"
							 "\treturn FreeKeyOf(Event, true) ? FReply::Handled() : FReply::Unhandled();\n}\n",
				"OnKeyDown");
			Need(Up == "FReply SSeaBattleScreen::OnKeyUp(const FGeometry& Geometry, const FKeyEvent& Event)\n{\n\treturn FreeKeyOf(Event, false) ? FReply::Handled() : FReply::Unhandled();\n}\n", "OnKeyUp");
			Need(CountOf(Keys, "return ") == 3 && Has(Keys, "\tif (!Battle.IsValid() || Battle->IsMonthScene() || !Battle->HasFreeEye())\n\t{\n\t\treturn false;\n\t}\n") &&
					Has(Keys, "\tif (Name == KeyName::Other)\n\t{\n\t\treturn false;\n\t}\n") &&
					Has(Keys, "\treturn Battle->FreeKey(Name, bDown, Event.IsShiftDown(), Event.IsRepeat(), Event.IsControlDown() || Event.IsCommandDown() || Event.IsAltDown());\n}\n"),
				"FreeKeyOf");
			Need(Has(SceneKey, "\tconst bool bHandled = SeaFreeEye::ScreenKey(FreeState, Key, bDown, bShift, bRepeat, bChord, Taken);\n") && Has(SceneKey, "\treturn bHandled;\n}\n") &&
					CountOf(SceneKey, "return ") == 2,
				"FreeKey");
			Need(Has(Lost, "\t\tBattle->FreeLetGoKeys();") && Has(SceneLost, "\tSeaFreeEye::ScreenLostKeys(FreeState);\n"), "focus lost");
			Need(Has(Wheel, "\tBattle->FreeWheel(Event.GetWheelDelta());\n") && CountOf(Wheel, "FreeWheel(") == 1 && Has(SceneWheel, "\t\tFreeState.H.Wheel += Notches;\n") &&
					CountOf(SceneWheel, "Wheel") == 2,
				"the wheel");
			Check("C8", "the screen's keys: OnKeyDown and OnKeyUp answer Handled only as the glue's ScreenKey says (no other key, L among them), the chord and the focus lost handed on; the wheel's notches handed on with their sign",
				Wrong.empty(), Wrong.empty() ? "as the glue rules" : "NOT: " + Wrong);
		}
	}

	// ---- The hand-made cases ---------------------------------------------------------------------------------------

	void Keys()
	{
		std::printf("Keys (the glue's KeyEvent and InputOf)\n");
		using SeaFreeEye::Hands;
		struct Case
		{
			KeyName Key;
			bool bShift;
			uint32_t Want;
		};
		const Case Cases[] = { { KeyName::W, false, SeaFreeEye::PanAhead }, { KeyName::Up, false, SeaFreeEye::PanAhead }, { KeyName::S, false, SeaFreeEye::PanBack },
			{ KeyName::Down, false, SeaFreeEye::PanBack }, { KeyName::A, false, SeaFreeEye::PanLeft }, { KeyName::Left, false, SeaFreeEye::PanLeft },
			{ KeyName::D, false, SeaFreeEye::PanRight }, { KeyName::Right, false, SeaFreeEye::PanRight }, { KeyName::Q, false, SeaFreeEye::TurnLeft },
			{ KeyName::E, false, SeaFreeEye::TurnRight }, { KeyName::W, true, SeaFreeEye::TiltDown }, { KeyName::Up, true, SeaFreeEye::TiltDown },
			{ KeyName::S, true, SeaFreeEye::TiltUp }, { KeyName::Down, true, SeaFreeEye::TiltUp }, { KeyName::Q, true, SeaFreeEye::GoDown }, { KeyName::E, true, SeaFreeEye::GoUp },
			{ KeyName::A, true, SeaFreeEye::PanLeft }, { KeyName::D, true, SeaFreeEye::PanRight } };
		int Wrong = 0;
		std::string First;
		for (const Case& C : Cases)
		{
			Hands H;
			const bool bMine = SeaFreeEye::KeyEvent(H, C.Key, true, C.bShift, false) == SeaFreeEye::KeyTaken::Mine;
			const uint32_t Pressed = H.Bits;
			SeaFreeEye::KeyEvent(H, C.Key, true, !C.bShift, true); // (a repeat with Shift changed: the latch holds)
			const uint32_t Repeated = H.Bits;
			SeaFreeEye::KeyEvent(H, C.Key, false, false, false);
			if (!bMine || Pressed != C.Want || Repeated != C.Want || H.Bits != 0u)
			{
				++Wrong;
				First = First.empty() ? "key " + std::to_string(static_cast<int>(C.Key)) + (C.bShift ? " with Shift" : "") : First;
			}
		}
		Check("C8", "each held key's meaning (arrows as W A S D; Shift+W, Shift+S and Shift with the up and down arrows tilt; Shift+Q, Shift+E down and up; Q, E turn), latched at its press through repeats, cleared at its release",
			Wrong == 0, std::to_string(std::size(Cases) - Wrong) + " of " + std::to_string(std::size(Cases)) + (First.empty() ? "" : "; " + First));
		Hands Tilt;
		SeaFreeEye::KeyEvent(Tilt, KeyName::W, true, true, false);
		SeaFreeEye::KeyEvent(Tilt, KeyName::S, true, false, false);
		SeaFreeEye::KeyEvent(Tilt, KeyName::S, false, false, false);
		const FreeEye::Input TiltIn = SeaFreeEye::InputOf(Tilt, 0.1, 16.0 / 9.0);
		Hands Turn;
		SeaFreeEye::KeyEvent(Turn, KeyName::Q, true, false, false);
		const FreeEye::Input TurnIn = SeaFreeEye::InputOf(Turn, 0.1, 16.0 / 9.0), LongIn = SeaFreeEye::InputOf(Turn, 3.0, 16.0 / 9.0);
		Hands Rise;
		SeaFreeEye::KeyEvent(Rise, KeyName::E, true, true, false);
		const FreeEye::Input RiseIn = SeaFreeEye::InputOf(Rise, 0.1, 16.0 / 9.0);
		Check("C8", "Shift+W never pans (it tilts at 50 degrees a second); Q turns at 75 degrees a second (a long frame held to the core's quarter second); Shift+E rises",
			TiltIn.PanAhead == 0.0 && TiltIn.PanRight == 0.0 && std::abs(TiltIn.Tilt - 5.0) < 1e-12 && std::abs(TurnIn.Turn + 7.5) < 1e-12 && std::abs(LongIn.Turn + 18.75) < 1e-12 &&
				RiseIn.Rise == 1.0 && RiseIn.Turn == 0.0,
			"tilt " + Num(TiltIn.Tilt, 2) + ", turn " + Num(TurnIn.Turn, 2) + " (" + Num(LongIn.Turn, 2) + " over 3 s), rise " + Num(RiseIn.Rise, 0));
		Hands P;
		SeaFreeEye::KeyEvent(P, KeyName::J, true, false, false);
		const bool bJ1 = P.bJ;
		P.bJ = false;
		SeaFreeEye::KeyEvent(P, KeyName::J, true, false, true);
		const bool bJRepeat = P.bJ;
		SeaFreeEye::KeyEvent(P, KeyName::C, true, false, false);
		const bool bC1 = P.bC;
		P.bC = false;
		SeaFreeEye::KeyEvent(P, KeyName::C, true, false, true);
		const bool bCRepeat = P.bC;
		SeaFreeEye::KeyEvent(P, KeyName::Tab, true, false, false);
		SeaFreeEye::KeyEvent(P, KeyName::Tab, true, false, true);
		SeaFreeEye::KeyEvent(P, KeyName::Tab, true, true, false);
		SeaFreeEye::KeyEvent(P, KeyName::Tab, false, false, false);
		const int Tabs = P.Tab;
		const bool bU = SeaFreeEye::KeyEvent(P, KeyName::U, true, false, false) == SeaFreeEye::KeyTaken::Under;
		const bool bURepeat = SeaFreeEye::KeyEvent(P, KeyName::U, true, false, true) == SeaFreeEye::KeyTaken::Under;
		const bool bShiftU = SeaFreeEye::KeyEvent(P, KeyName::U, true, true, false) == SeaFreeEye::KeyTaken::Under;
		const bool bOther = SeaFreeEye::KeyEvent(P, KeyName::Other, true, false, false) == SeaFreeEye::KeyTaken::NotMine;
		Check("C8", "J and C once a press (a held key's repeats never toggle again); Tab steps on every press and repeat (Shift+Tab back); U and Shift+U go under, never on a repeat; any other key is not the camera's",
			bJ1 && !bJRepeat && bC1 && !bCRepeat && Tabs == 1 && bU && !bURepeat && bShiftU && bOther,
			"J " + std::to_string(bJ1) + std::to_string(bJRepeat) + ", C " + std::to_string(bC1) + std::to_string(bCRepeat) + ", Tab " + std::to_string(Tabs) + ", U " + std::to_string(bU) +
				std::to_string(bURepeat) + std::to_string(bShiftU) + ", other " + std::to_string(bOther));

		// C8: the screen's answer (the glue's ScreenKey, critic mark C10), every key both ways. The rule written again: a camera key's plain
		// press Handled; a press with Control, Command or Option held never, unless the screen already holds that key (its
		// repeats); a release always told to the free eye (nothing stays held) but Handled only when the screen took its
		// press; Other (L, Escape, every campaign key) never, either way; the focus lost lets go of all and holds none.
		int Answers = 0, Right = 0;
		std::string Miss;
		auto Expect = [&Answers, &Right, &Miss](bool bGot, bool bWant, const std::string& What)
		{
			++Answers;
			Right += bGot == bWant ? 1 : 0;
			Miss += bGot == bWant || !Miss.empty() ? "" : What + (bGot ? " Handled" : " Unhandled");
		};
		const KeyName AllKeys[] = { KeyName::W, KeyName::A, KeyName::S, KeyName::D, KeyName::Up, KeyName::Down, KeyName::Left, KeyName::Right, KeyName::Q,
			KeyName::E, KeyName::J, KeyName::C, KeyName::Tab, KeyName::U, KeyName::Other };
		bool bNothingHeld = true, bDive = true;
		for (const KeyName K : AllKeys)
		{
			const bool bCamera = K != KeyName::Other;
			const std::string Name = "key " + std::to_string(static_cast<int>(K));
			SeaFreeEye::KeyTaken Out = SeaFreeEye::KeyTaken::NotMine;
			// (a plain press and its release)
			SeaFreeEye::State A;
			Expect(SeaFreeEye::ScreenKey(A, K, true, false, false, false, Out), bCamera, Name + " pressed");
			bDive = bDive && (K == KeyName::U) == (Out == SeaFreeEye::KeyTaken::Under);
			Expect(SeaFreeEye::ScreenKey(A, K, true, false, true, true, Out), bCamera, Name + " repeated with Control held");
			bDive = bDive && Out != SeaFreeEye::KeyTaken::Under;
			Expect(SeaFreeEye::ScreenKey(A, K, false, false, false, false, Out), bCamera, Name + " let go");
			bNothingHeld = bNothingHeld && A.H.Bits == 0u && A.ScreenKeys == 0u;
			// (a chord: its press the campaign's, and so its release, even let go after Control is; nothing held after it)
			SeaFreeEye::State B;
			Expect(SeaFreeEye::ScreenKey(B, K, true, false, false, true, Out), false, Name + " pressed with Control");
			bDive = bDive && Out != SeaFreeEye::KeyTaken::Under;
			Expect(SeaFreeEye::ScreenKey(B, K, true, false, true, true, Out), false, Name + " repeated with Control from its press");
			Expect(SeaFreeEye::ScreenKey(B, K, false, false, false, false, Out), false, Name + " let go after a chord");
			Expect(SeaFreeEye::ScreenKey(B, K, false, false, false, true, Out), false, Name + " let go again");
			bNothingHeld = bNothingHeld && B.H.Bits == 0u && B.ScreenKeys == 0u;
			// (a key that went down before the screen had the keys, or the screen lost them while it was down: its release
			// the campaign's, and nothing held)
			SeaFreeEye::State Lost;
			Expect(SeaFreeEye::ScreenKey(Lost, K, false, false, false, false, Out), false, Name + " let go with no press seen");
			SeaFreeEye::ScreenKey(Lost, K, true, true, false, false, Out);
			SeaFreeEye::ScreenLostKeys(Lost);
			bNothingHeld = bNothingHeld && Lost.H.Bits == 0u && Lost.ScreenKeys == 0u;
			Expect(SeaFreeEye::ScreenKey(Lost, K, false, true, false, false, Out), false, Name + " let go after the focus was lost");
		}
		Check("C8", "the screen's answer for every key both ways: a camera key's own press and release Handled; a chord's press, its release and a release with no press seen Unhandled; any other key (L among them) never; nothing left held; Ctrl+U never dives",
			Right == Answers && bNothingHeld && bDive, std::to_string(Right) + " of " + std::to_string(Answers) + (Miss.empty() ? "" : "; " + Miss) + (bNothingHeld ? "" : "; a key left held") + (bDive ? "" : "; a dive on a chord or a repeat"));
		// (the wheel: a notch + is in, the boom shorter)
		{
			Rig R;
			R.Key(KeyName::W, true);
			R.Frame(1.0 / 60.0);
			R.Key(KeyName::W, false);
			R.Run(0.5);
			const double Was = R.Free.Arm();
			R.St.H.Wheel += 1.0;
			R.Run(1.0);
			Hands Notch;
			Notch.Wheel = 1.0;
			const double Zoomed = SeaFreeEye::InputOf(Notch, 1.0 / 60.0, 16.0 / 9.0).Zoom;
			Check("C8", "a wheel notch + (the screen's GetWheelDelta, handed on unchanged) brings the boom in", Zoomed == 1.0 && R.Free.Arm() < Was, Num(Was, 1) + " m to " + Num(R.Free.Arm(), 1) + " m");
		}
	}

	void Switches()
	{
		std::printf("The switches (SeaFreeEye::Take)\n");
		using namespace SeaLookSwitches;
		auto Asking = [](const char* Free, const char* Ride, const char* Drive)
		{
			Look L;
			for (int K = 0; K < Count; ++K)
			{
				const std::string Name = All[K].Name;
				const char* Value = Name == "TheaterSeaFreeEye=" ? Free : Name == "TheaterSeaFreeEyeRide=" ? Ride : Name == "TheaterSeaFreeEyeDrive=" ? Drive : nullptr;
				if (Value)
				{
					L.Given.push_back(Asked{ K, Value });
				}
			}
			return L;
		};
		auto Said = [](const Look& L)
		{
			std::string Out;
			for (const Asked& One : L.Given)
			{
				Out += (Out.empty() ? "" : " ") + One.Value;
			}
			return Out;
		};
		Look Full = Asking("1", "1", "tour"), Month = Asking("1", "1", "tour"), Off = Asking("0", "1", "tour"), Junk = Asking("yes", "1", "Tour"), File = Asking("1", "0", "/tmp/a drive");
		const SeaFreeEye::Taken TFull = SeaFreeEye::Take(Full, true), TMonth = SeaFreeEye::Take(Month, false), TOff = SeaFreeEye::Take(Off, true), TJunk = SeaFreeEye::Take(Junk, true),
								TFile = SeaFreeEye::Take(File, true);
		Look None;
		const SeaFreeEye::Taken TNone = SeaFreeEye::Take(None, true);
		const bool bOk = TFull.bOn && TFull.bRide && TFull.Drive == "tour" && Said(Full) == "1 1 tour" && !TMonth.bOn && !TMonth.bRide && TMonth.Drive == "off" && Said(Month) == "0 0 off" &&
			!TOff.bOn && !TOff.bRide && TOff.Drive == "off" && Said(Off) == "0 0 off" && !TJunk.bOn && Said(Junk) == "0 0 off" && TFile.bOn && !TFile.bRide && TFile.Drive == "/tmp/a drive" &&
			!TNone.bOn && TNone.Drive == "off";
		Check("C11", "on only in a full battle watched live (never the month's scene or a picture run); Ride and Drive only with it; the values in force written back",
			bOk, "full: " + Said(Full) + "; month: " + Said(Month) + "; off: " + Said(Off) + "; junk: " + Said(Junk) + "; a file: " + Said(File));
		const std::string Plain = Line(0, {}), OnLine = Line(Full);
		Check("C11", "the \"Sea look:\" line says them, off as 0 / off and on as asked",
			Has(Plain, "-TheaterSeaFreeEye=0, -TheaterSeaFreeEyeRide=0, -TheaterSeaFreeEyeDrive=off") &&
				Has(OnLine, "-TheaterSeaFreeEye=1 (asked), -TheaterSeaFreeEyeRide=1 (asked), -TheaterSeaFreeEyeDrive=tour (asked)"),
			OnLine.substr(OnLine.find("-TheaterSeaFreeEye=")).substr(0, 90));
	}

	void Lens()
	{
		std::printf("The lens (Q8: the director's 50 and 55 degrees across held on every window shape)\n");
		const double A169 = 16.0 / 9.0;
		const double N169 = SeaFreeEye::VerticalLens(50.0, A169), F169 = SeaFreeEye::VerticalLens(55.0, A169);
		std::string Shapes;
		bool bBack = true;
		for (const double A : { A169, 1.6, 1.54 })
		{
			FreeEye::Settings S;
			SeaFreeEye::FitLens(S, A);
			const double Near = SeaFreeEye::HorizontalLens(S.FovNear, A), Far = SeaFreeEye::HorizontalLens(S.FovFar, A);
			bBack = bBack && std::abs(Near - 50.0) < 1e-9 && std::abs(Far - 55.0) < 1e-9;
			Shapes += (Shapes.empty() ? "" : "; ") + Num(A, 3) + ": " + Num(S.FovNear, 2) + " and " + Num(S.FovFar, 2) + " high, " + Num(Near, 1) + " and " + Num(Far, 1) + " across";
		}
		// (the plan's 29.4 and 32.6 are to one decimal: 32.6 itself is 54.93 across, so the way back is held on the unrounded
		// angles, exactly)
		Check("C12", "vertical to across: 50 across is 29.4 high and 55 is 32.6 at 16:9 (to the plan's one decimal), and back exactly; the core's lenses fitted so 50 and 55 hold across at 16:9, 1.6 and 1.54",
			std::abs(N169 - 29.4) < 0.05 && std::abs(F169 - 32.6) < 0.05 && std::abs(SeaFreeEye::HorizontalLens(N169, A169) - 50.0) < 1e-9 &&
				std::abs(SeaFreeEye::HorizontalLens(F169, A169) - 55.0) < 1e-9 && bBack,
			Shapes);
		// The ease: from the director's 15 and 85, at 24 and 60 frames a second and through hitches, never faster than 40 a second.
		double Worst = 0.0, Longest = 0.0;
		bool bArrive = true;
		for (const double From : { 15.0, 85.0, 50.0 })
		{
			for (const int Kind : { 0, 1, 2 })
			{
				double X = From, Swing = 0.0, T = 0.0;
				const double Want = From < 50.0 ? 50.0 : 55.0;
				int Frame = 0;
				while (T < 10.0)
				{
					const double Dt = Kind == 0 ? 1.0 / 24.0 : Kind == 1 ? 1.0 / 60.0 : (Frame % 50 == 20 ? 1.0 : Frame % 37 == 10 ? 0.3 : 1.0 / 60.0);
					const double Was = X;
					SeaFreeEye::EaseLens(X, Swing, Want, std::min(Dt, 0.25));
					Worst = std::max(Worst, std::abs(X - Was) / std::min(Dt, 0.25));
					T += Dt;
					++Frame;
					if (X == Want && Longest < T && std::abs(Was - Want) > 0.0)
					{
						Longest = T;
					}
				}
				bArrive = bArrive && X == Want;
			}
		}
		Check("C12", "the lens eased from the director's (15 and 85 among them) never faster than 40 degrees a real second, at 24 and 60 frames a second and through 0.3 and 1 s hitches, and arriving",
			Worst <= LensMostPerSecond + 1e-9 && bArrive, "fastest " + Num(Worst, 3) + " degrees a second, the longest ease " + Num(Longest, 2) + " s");
		// A take at each shape: the take's frame his lens exactly, then eased to 50..55 across.
		bool bSettle = true;
		std::string Settled;
		for (const double A : { A169, 1.6, 1.54 })
		{
			Rig R(A);
			R.In.DirLens = 15.0;
			R.Key(KeyName::W, true);
			const SeaFreeEye::Shot First = R.Frame(1.0 / 60.0);
			R.Key(KeyName::W, false);
			R.Run(6.0);
			bSettle = bSettle && First.bFree && First.Lens == 15.0 && R.Last.Lens >= 50.0 - 1e-6 && R.Last.Lens <= 55.0 + 1e-6;
			Settled += (Settled.empty() ? "" : ", ") + Num(R.Last.Lens, 2);
		}
		Check("C12", "a take keeps the director's lens on its frame, then the picture settles 50 to 55 degrees across at 16:9, 1.6 and 1.54", bSettle, Settled);
	}

	void Takes()
	{
		std::printf("The take, the give back, U (C13, C14, C19)\n");
		{
			Rig R;
			R.In.DirEye = Vec{ 9100.0, 9200.0, 40.0 };
			R.In.DirLook = Vec{ 9600.0, 9900.0, 0.0 };
			const SeaFreeEye::Shot Before = R.Frame(1.0 / 60.0);
			R.Key(KeyName::W, true);
			const SeaFreeEye::Shot First = R.Frame(1.0 / 60.0);
			const double Off = (First.Eye - R.In.DirEye).Length(), LookOff = (First.Look - R.In.DirLook).Length();
			const SeaFreeEye::Shot Second = R.Frame(1.0 / 60.0);
			Check("C13", "the first free frame is the director's shot: its eye and look point within 1 cm, its lens his; no cut; nothing before a key",
				!Before.bFree && First.bFree && First.bTaken && !First.bCut && Off <= 0.01 && LookOff <= 0.01 && First.Lens == R.In.DirLens && Second.bFree && !Second.bTaken,
				Num(100.0 * Off, 3) + " and " + Num(100.0 * LookOff, 3) + " cm");
		}
		{
			// A click on the water, and a pick alone, never take the camera; a double click on her picks and locks and takes it.
			Rig R;
			R.Ships = { MakeShip(64, 1, 0, 0, 9500.0, 9500.0, 30.0) };
			R.Frame(1.0 / 60.0);
			SeaFreeEye::ClickAt(R.St.H, R.RayTo(Vec{ 9300.0, 9700.0, 0.0 }), false);
			const SeaFreeEye::Shot Water = R.Frame(1.0 / 60.0);
			SeaFreeEye::ClickAt(R.St.H, R.RayTo(PlaceOf(R.Ships[0]) + Vec{ 0.0, 0.0, 4.0 }), false);
			const SeaFreeEye::Shot Picked = R.Frame(1.0 / 60.0);
			const int PickWas = R.St.PickKey;
			SeaFreeEye::ClickAt(R.St.H, R.RayTo(PlaceOf(R.Ships[0]) + Vec{ 0.0, 0.0, 4.0 }), true);
			const SeaFreeEye::Shot Locked = R.Frame(1.0 / 60.0);
			// (the take's frame his shot exactly, so the lock that took the camera is sent from the next frame: first with
			// no velocity, then with it)
			const bool bTakeBare = R.St.bFed && !R.St.Fed.bLock;
			R.Frame(1.0 / 60.0);
			const bool bLockFirst = bTakeBare && R.St.bFed && R.St.Fed.bLock && !R.St.Fed.bAnchorVel;
			R.Frame(1.0 / 60.0);
			const bool bVelNext = R.St.bFed && R.St.Fed.bLock && R.St.Fed.bAnchorVel;
			// C: nothing survives; the next W's take sends no lock; Tab then starts from the top.
			R.Key(KeyName::C, true);
			R.Key(KeyName::C, false);
			const SeaFreeEye::Shot Back = R.Frame(1.0 / 60.0);
			const bool bClean = !R.St.L.On() && R.St.PickKey < 0 && R.St.PickPiece < 0 && !R.St.bOn && !Back.bFree && Back.bGiveBack;
			R.Key(KeyName::W, true);
			const SeaFreeEye::Shot Again = R.Frame(1.0 / 60.0);
			const bool bAgainBare = Again.bTaken && R.St.bFed && !R.St.Fed.bLock;
			const SeaFreeEye::Shot After = R.Frame(1.0 / 60.0);
			const bool bNoLock = bAgainBare && After.bFree && R.St.bFed && !R.St.Fed.bLock && After.Locked < 0;
			R.Key(KeyName::W, false);
			Check("C19", "a click on open water and a pick alone never take the camera; a double click locks (the take's frame sends no lock, the next the lock with no velocity, the one after with it)",
				!Water.bFree && !Picked.bFree && PickWas == 64 && Locked.bFree && Locked.bTaken && Locked.Locked == 64 && bLockFirst && bVelNext,
				"pick " + std::to_string(PickWas) + ", locked " + std::to_string(Locked.Locked));
			Check("C19", "C gives back with the lock, the pick, the Tab place and the division cleared, the director's first frame back a cut; the next W's take sends no lock, then or after",
				bClean && bNoLock, std::string(bClean ? "cleared" : "NOT cleared") + ", " + (bNoLock ? "no lock" : "a lock") + " on the next take");
		}
		{
			// U while free: given back first (Q9), then his dive the director's; keys held through it take again after it.
			Rig R;
			R.Ships = { MakeShip(64, 1, 0, 0, 9500.0, 9500.0, 30.0) };
			R.Key(KeyName::Tab, true);
			R.Key(KeyName::Tab, false);
			R.Frame(1.0 / 60.0);
			R.Run(1.0);
			bool bDive = false;
			const bool bHad = SeaFreeEye::KeyEvent(R.St.H, KeyName::U, true, false, false) == SeaFreeEye::KeyTaken::Under && SeaFreeEye::UnderPressed(R.St, bDive);
			const bool bGiven = !R.St.bOn && !R.St.L.On() && bDive;
			R.In.bHandDive = true;
			R.In.DirEye = Vec{ 9400.0, 9400.0, -10.0 };
			R.In.DirLook = Vec{ 9450.0, 9450.0, -12.0 };
			R.Key(KeyName::W, true);
			bool bHeld = true;
			for (int K = 0; K < 120; ++K)
			{
				bHeld = bHeld && !R.Frame(1.0 / 60.0).bFree;
			}
			R.In.bHandDive = false;
			R.In.DirEye = Vec{ 9300.0, 9300.0, 30.0 };
			R.In.DirLook = Vec{ 9500.0, 9500.0, 0.0 };
			const SeaFreeEye::Shot After = R.Frame(1.0 / 60.0);
			R.Key(KeyName::W, false);
			Check("C14", "U while he has the free camera gives it back first (the lock let go) and goes under; through his dive the director has the camera; a key held through it takes the camera again from the director's shot after it",
				bHad && bGiven && bHeld && After.bFree && After.bTaken && !After.bCut && (After.Eye - R.In.DirEye).Length() < 0.01,
				std::string(bHad ? "given back" : "NOT given back") + ", " + (bHeld ? "held" : "NOT held") + ", " + (After.bTaken ? "taken after" : "NOT taken after"));
			// A take while the director stands under the water: a cut up through the face, the eye clear.
			Rig U;
			U.In.DirEye = Vec{ 9400.0, 9400.0, -8.0 };
			U.In.DirLook = Vec{ 9450.0, 9480.0, -10.0 };
			U.Key(KeyName::W, true);
			const SeaFreeEye::Shot Up = U.Frame(1.0 / 60.0);
			// During a drive, U does nothing.
			Rig D;
			std::string Error;
			SeaFreeEye::ParseDrive(SeaFreeEye::TourText(), D.St.D, Error);
			D.Frame(1.0 / 60.0);
			bool bDriveDive = true;
			const bool bDriveHad = SeaFreeEye::UnderPressed(D.St, bDriveDive);
			Check("C14", "a take from the director's eye under the water is a cut with the eye at least 0.5 m over the sea; while a drive plays U does nothing",
				Up.bFree && Up.bTaken && Up.bCut && Up.Eye.Z >= HardClear - 1e-9 && !bDriveHad && !bDriveDive, "the eye at " + Num(Up.Eye.Z, 2) + " m");
		}
	}

	void Picks()
	{
		std::printf("Picks, locks, Tab, divisions, let go (C3 to C7)\n");
		{
			// A stale pick: she is lost from sight; the pick goes that frame; J then sends nothing (the pose as without J).
			Rig R, Twin;
			for (Rig* Each : { &R, &Twin })
			{
				Each->Ships = { MakeShip(64, 1, 1, 0, 9500.0, 9500.0, 30.0) };
				Each->Key(KeyName::W, true);
				Each->Frame(1.0 / 60.0);
				Each->Key(KeyName::W, false);
				Each->Run(0.5);
				SeaFreeEye::ClickAt(Each->St.H, Each->RayTo(PlaceOf(Each->Ships[0]) + Vec{ 0.0, 0.0, 4.0 }), false);
				Each->Frame(1.0 / 60.0);
			}
			const int Picked = R.St.PickKey;
			for (Rig* Each : { &R, &Twin })
			{
				Each->Ships[0].bShown = false;
				Each->Frame(1.0 / 60.0);
			}
			const int Kept = R.St.PickKey;
			R.Key(KeyName::J, true);
			R.Key(KeyName::J, false);
			const SeaFreeEye::Shot WithJ = R.Frame(1.0 / 60.0), WithoutJ = Twin.Frame(1.0 / 60.0);
			bool bSame = (WithJ.Eye - WithoutJ.Eye).Length() == 0.0 && (WithJ.Look - WithoutJ.Look).Length() == 0.0;
			for (int K = 0; K < 60; ++K)
			{
				const SeaFreeEye::Shot A = R.Frame(1.0 / 60.0), B = Twin.Frame(1.0 / 60.0);
				bSame = bSame && (A.Eye - B.Eye).Length() == 0.0 && (A.Look - B.Look).Length() == 0.0 && !(R.St.bFed && R.St.Fed.bLock);
			}
			Check("C3", "a pick whose ship is lost from sight is dropped that frame, and J then sends no lock (no pose differs from the same hands without J)",
				Picked == 64 && Kept == -1 && !R.St.L.On() && bSame, "picked " + std::to_string(Picked) + ", then " + std::to_string(Kept) + (bSame ? ", poses the same" : ", poses DIFFER"));
		}
		{
			// J with nothing picked or locked (the lead's default for Q3), a boat he has not found standing at the very middle
			// of the picture and a found ship 120 m to the right of it: the found ship is locked, and every frame is the same
			// to the bit as with the hidden boat deleted (C3: an unfound boat is never read, not even to be passed over).
			Rig R, Twin;
			const ShipOut Found = MakeShip(64, 1, 0, 0, 9696.0, 9728.0, 30.0);
			ShipOut Hidden = MakeShip(128, 2, 1, 0, 9600.0, 9800.0, 30.0);
			Hidden.bShown = false;
			R.Ships = { Hidden, Found };
			Twin.Ships = { Found };
			for (Rig* Each : { &R, &Twin })
			{
				Each->Frame(1.0 / 60.0);
				Each->Key(KeyName::J, true);
				Each->Key(KeyName::J, false);
			}
			bool bSame = true;
			int Frames = 0;
			for (int K = 0; K < 180; ++K)
			{
				const SeaFreeEye::Shot A = R.Frame(1.0 / 60.0), B = Twin.Frame(1.0 / 60.0);
				bSame = bSame && A.bFree == B.bFree && A.Locked == B.Locked && A.bCut == B.bCut && (A.Eye - B.Eye).Length() == 0.0 && (A.Look - B.Look).Length() == 0.0 &&
					A.Lens == B.Lens;
				Frames += A.bFree ? 1 : 0;
			}
			Check("C3", "J with nothing picked locks the found ship nearest the middle of the picture though a hidden boat stands nearer it, every frame the same to the bit as with the hidden boat deleted",
				R.St.L.Key == 64 && Twin.St.L.Key == 64 && Frames == 180 && bSame,
				"locked " + std::to_string(R.St.L.Key) + " (" + std::to_string(Twin.St.L.Key) + " with it deleted), " + std::to_string(Frames) + " free frames" + (bSame ? ", the same" : ", DIFFERENT"));
		}
		{
			// J after a Tab lets go (the UI director's words while locked: PRESS J AGAIN, TO LET GO): a click picks one ship,
			// Tab locks another (the first in Tab's order), and every lock moves the pick onto what it locks, so an older
			// click's pick never turns that J into a lock on her.
			Rig R;
			R.Ships = { MakeShip(64, 1, 0, 0, 9500.0, 9500.0, 30.0), MakeShip(65, 1, 0, 1, 9800.0, 9300.0, 30.0) };
			R.Frame(1.0 / 60.0);
			SeaFreeEye::ClickAt(R.St.H, R.RayTo(PlaceOf(R.Ships[1]) + Vec{ 0.0, 0.0, 4.0 }), false);
			R.Frame(1.0 / 60.0);
			const int Picked = R.St.PickKey;
			R.Key(KeyName::Tab, true);
			R.Key(KeyName::Tab, false);
			R.Frame(1.0 / 60.0);
			const int Tabbed = R.St.L.Key, PickAfter = R.St.PickKey;
			R.Run(1.0);
			R.Key(KeyName::J, true);
			R.Key(KeyName::J, false);
			const SeaFreeEye::Shot Go = R.Frame(1.0 / 60.0);
			const int After = R.St.L.On() ? R.St.L.Key : -1;
			Check("C5", "a click picks 65, Tab locks 64 (the first in Tab's order) and the pick moves onto her, so J then lets go, never locks the older pick",
				Picked == 65 && Tabbed == 64 && PickAfter == 64 && After == -1 && Go.Locked < 0 && Go.bFree,
				"picked " + std::to_string(Picked) + ", Tab locked " + std::to_string(Tabbed) + " (the pick then " + std::to_string(PickAfter) + "), after J locked " + std::to_string(After));
		}
		{
			// A take by a lock with the director's eye inside her clearance (half her drawn length: 75 m for 150 m): the take
			// his shot exactly, and the clearance grown from where the eye stood at 20 m a real second, never a throw.
			Rig R;
			R.Ships = { MakeShip(64, 1, 0, 0, 9500.0, 9500.0, 0.0) };
			R.Ships[0].Speed = 0.0;
			R.In.DirEye = Vec{ 9530.0, 9460.0, 20.0 };
			R.In.DirLook = Vec{ 9500.0, 9500.0, 0.0 };
			const double Start = (R.In.DirEye - PlaceOf(R.Ships[0])).Length();
			R.Key(KeyName::Tab, true);
			R.Key(KeyName::Tab, false);
			const SeaFreeEye::Shot First = R.Frame(1.0 / 60.0);
			const bool bTake = First.bTaken && First.Locked == 64 && !First.bCut && (First.Eye - R.In.DirEye).Length() <= 0.01;
			double Least = 1e9, Step = 0.0, Fed = 0.0, Grew = 0.0;
			Vec Was = First.Eye;
			double FedWas = -1.0;
			for (int K = 0; K < 60 * 8; ++K)
			{
				const SeaFreeEye::Shot O = R.Frame(1.0 / 60.0);
				Least = std::min(Least, (O.Eye - PlaceOf(R.Ships[0])).Length());
				Step = std::max(Step, (O.Eye - Was).Length());
				// (from the lock's first frame sent, the frame after the take)
				Grew = FedWas >= 0.0 ? std::max(Grew, (R.St.Fed.AnchorClear - FedWas) * 60.0) : Grew;
				FedWas = R.St.Fed.bLock ? R.St.Fed.AnchorClear : -1.0;
				Fed = R.St.Fed.AnchorClear;
				Was = O.Eye;
			}
			const double End = (Was - PlaceOf(R.Ships[0])).Length();
			Check("C13", "a take by a lock with the eye inside her clearance: his shot on the take's frame, the clearance grown from there no faster than 20 m a real second to all of it, the eye never thrown",
				bTake && Least >= Start - 0.01 && Grew <= 20.0 + 1e-6 && std::abs(Fed - 75.0) < 1e-9 && End >= 75.0 - 0.05 && Step <= 0.5,
				"from " + Num(Start, 1) + " m to " + Num(End, 2) + " m of 75, fed growing at most " + Num(Grew, 2) + " m/s, the most the eye moved in a frame " + Num(Step, 3) + " m");
		}
		{
			// Let go: a locked ship lost from sight lets go that frame, no cut, and the view holds within the core's coast.
			Rig R;
			R.Ships = { MakeShip(64, 1, 0, 0, 9500.0, 9500.0, 90.0), MakeShip(128, 2, 1, 0, 14000.0, 14000.0, 0.0) };
			R.Ships[0].Speed = 15.0;
			R.Key(KeyName::Tab, true);
			R.Key(KeyName::Tab, false);
			for (int K = 0; K < 600; ++K)
			{
				R.Move(1.0 / 60.0, 1.0);
				R.Frame(1.0 / 60.0);
			}
			const bool bOn = R.St.L.Key == 64;
			R.Ships[0].bShown = false;
			R.Move(1.0 / 60.0, 1.0);
			const SeaFreeEye::Shot Go = R.Frame(1.0 / 60.0);
			const bool bSent = R.St.bFed && R.St.Fed.bLock;
			const Vec Held = R.Free.Pivot();
			double Coast = 0.0;
			for (int K = 0; K < 360; ++K)
			{
				R.Move(1.0 / 60.0, 1.0);
				R.Frame(1.0 / 60.0);
				Coast = std::max(Coast, (R.Free.Pivot() - Held).Length());
			}
			Check("C4", "a locked ship lost from sight lets go that frame, with no cut, and the view holds where it is (it coasts a few metres and stops)",
				bOn && !bSent && !Go.bCut && Go.bFree && Coast <= 5.0, "coasted " + Num(Coast, 2) + " m at 15 m/s");
		}
		{
			// Let go (J) with his wheel wanting a boom far shorter than her clearance (12 notches in while locked, as the
			// tour's low pan leaves it): her clearance held the boom off her, and the let go holds it where it stands (1g,
			// "the view holds"); dropped at once, the boom would shrink to the wheel's 2 m round her middle and put the eye
			// in her hull. Measured on the eye, not the pivot: it moves no more than 3 m in 6 s and never enters her hull.
			Rig R;
			R.Ships = { MakeShip(64, 1, 0, 0, 9500.0, 9500.0, 0.0) };
			R.Ships[0].Speed = 0.0;
			R.Key(KeyName::Tab, true);
			R.Key(KeyName::Tab, false);
			R.Run(6.0);
			for (int K = 0; K < 60 * 4; ++K)
			{
				R.St.H.Wheel += K % 20 == 0 ? 1.0 : 0.0;
				R.Frame(1.0 / 60.0);
			}
			R.Run(4.0);
			const bool bOn = R.St.L.Key == 64 && R.Free.Locked() == 64;
			const double ArmWas = R.Free.Arm(), WantWas = R.Free.ArmOf(R.Free.WantZoom());
			const Vec EyeWas = R.Last.Eye;
			R.Key(KeyName::J, true);
			R.Key(KeyName::J, false);
			const SeaFreeEye::Shot Go = R.Frame(1.0 / 60.0);
			const ShipOut& Her = R.Ships[0];
			auto InHull = [&Her](const Vec& Eye)
			{
				// (her hull's box as the test draws it: her length by her beam, 8 m under her waterline to 0.12 of her length
				// and 6 m over it)
				const Vec O = Eye - PlaceOf(Her);
				const double H = Her.Heading * Rad;
				const double Fore = O.X * std::sin(H) + O.Y * std::cos(H), Side = O.X * std::cos(H) - O.Y * std::sin(H);
				return std::abs(Fore) <= 0.5 * Her.Long && std::abs(Side) <= 0.5 * Her.Beam && O.Z >= -8.0 && O.Z <= 6.0 + 0.12 * Her.Long;
			};
			double Drift = 0.0, Nearest = 1e9;
			int Inside = InHull(Go.Eye) ? 1 : 0;
			for (int K = 0; K < 60 * 6; ++K)
			{
				const SeaFreeEye::Shot O = R.Frame(1.0 / 60.0);
				Drift = std::max(Drift, (O.Eye - EyeWas).Length());
				Nearest = std::min(Nearest, (O.Eye - PlaceOf(Her)).Length());
				Inside += InHull(O.Eye) ? 1 : 0;
			}
			Check("C4", "a let go (J) with his wheel wanting the boom far shorter than her clearance holds the boom where it stands: the eye moves no more than 3 m in 6 s and never enters her hull",
				bOn && WantWas < 0.25 * ArmWas && !R.St.L.On() && Go.bFree && !Go.bCut && Drift <= 3.0 && Inside == 0 && std::abs(R.Free.Arm() - ArmWas) <= 0.03 * ArmWas,
				"the boom " + Num(ArmWas, 1) + " m (his wheel wanting " + Num(WantWas, 1) + " m) to " + Num(R.Free.Arm(), 1) + " m, the eye moved at most " + Num(Drift, 2) +
					" m, its nearest to her middle " + Num(Nearest, 1) + " m, " + std::to_string(Inside) + " frames inside her hull");
			// Sinking and gone do the same; a ship that leaves the show's list too.
			int Right = 0;
			for (const int How : { 0, 1, 2 })
			{
				Rig S;
				S.Ships = { MakeShip(64, 1, 0, 0, 9500.0, 9500.0, 90.0) };
				S.Key(KeyName::Tab, true);
				S.Key(KeyName::Tab, false);
				S.Run(1.0);
				if (How == 0) S.Ships[0].Doing = Fate::Sinking;
				if (How == 1) S.Ships[0].Doing = Fate::Gone;
				if (How == 2) S.Ships.clear();
				const SeaFreeEye::Shot Gone = S.Frame(1.0 / 60.0);
				Right += !(S.St.bFed && S.St.Fed.bLock) && !Gone.bCut && !S.St.L.On() ? 1 : 0;
				// (and she can never be picked, locked or stepped to again)
				SeaFreeEye::KeyEvent(S.St.H, KeyName::Tab, true, false, false);
				S.Frame(1.0 / 60.0);
				Right -= S.St.L.On() ? 1 : 0;
			}
			Check("C4", "a ship that starts sinking, is gone or leaves the show lets go the same frame, with no cut, and Tab never reaches her", Right == 3, std::to_string(Right) + " of 3");
		}
		{
			// Tab: own side first, by line, piece, slot; wraps; Shift+Tab back; nothing locked: the first; never a hidden ship.
			// (the lines run against the keys, and a ship in no line has the lowest key: an order by key differs)
			Rig R;
			R.Ships = { MakeShip(64 * 3 + 1, 3, 0, 1, 9000.0, 9100.0, 0.0, 0), MakeShip(64 * 3 + 0, 3, 0, 0, 9000.0, 9200.0, 0.0, 0), MakeShip(64 * 2 + 0, 2, 0, 0, 9100.0, 9300.0, 0.0, 1),
				MakeShip(64 * 5 + 0, 5, 1, 0, 12000.0, 12000.0, 180.0, 0), MakeShip(64 * 6 + 0, 6, 1, 0, 12100.0, 12100.0, 180.0, -1), MakeShip(64 * 4 + 0, 4, 0, 0, 9200.0, 9300.0, 0.0, -1),
				MakeShip(64 * 7 + 0, 7, 1, 0, 12500.0, 12500.0, 180.0, 1), MakeShip(64 * 1 + 0, 1, 0, 0, 9300.0, 9300.0, 0.0, -1) };
			R.Ships[6].bShown = false;
			const std::vector<int> Want = { 192, 193, 128, 64, 256, 320, 384 }, Got = SeaFreeEye::TabOrder(R.Ships, 0);
			std::vector<int> Steps;
			for (const int Step : { 1, 1, 1, 1, 1, 1, 1, 1, -1, -1 })
			{
				SeaFreeEye::KeyEvent(R.St.H, KeyName::Tab, true, Step < 0, false);
				R.Frame(1.0 / 60.0);
				Steps.push_back(R.St.L.Key);
			}
			const std::vector<int> WantSteps = { 192, 193, 128, 64, 256, 320, 384, 192, 384, 320 };
			Rig Back;
			Back.Ships = R.Ships;
			SeaFreeEye::KeyEvent(Back.St.H, KeyName::Tab, true, true, false);
			Back.Frame(1.0 / 60.0);
			std::string Said;
			for (const int K : Steps)
			{
				Said += (Said.empty() ? "" : " ") + std::to_string(K);
			}
			Check("C5", "Tab's order: his side first, then the enemy; by line (none last), piece, slot; a hidden ship never in it; it wraps, Shift+Tab goes back, and with nothing locked Shift+Tab takes the last",
				Got == Want && Got == Recount(R.Ships, 0) && Steps == WantSteps && Back.St.L.Key == 384, Said);
		}
		{
			// A division: a click on the water near its line; a new id on every change of its members, never otherwise.
			Rig R;
			R.Ships = { MakeShip(64 * 2 + 0, 2, 0, 0, 9500.0, 9500.0, 0.0), MakeShip(64 * 2 + 1, 2, 0, 1, 9500.0, 9200.0, 0.0), MakeShip(64 * 2 + 2, 2, 0, 2, 9500.0, 8900.0, 0.0),
				MakeShip(64 * 9, 9, 1, 0, 9800.0, 9200.0, 0.0) };
			R.Ships[3].bShown = false;
			R.In.DirEye = Vec{ 9000.0, 9200.0, 300.0 };
			R.In.DirLook = Vec{ 9500.0, 9200.0, 0.0 };
			R.Frame(1.0 / 60.0);
			SeaFreeEye::ClickAt(R.St.H, R.RayTo(Vec{ 9540.0, 9050.0, 0.0 }), true);
			R.Frame(1.0 / 60.0);
			R.Frame(1.0 / 60.0); // (the take's frame his shot, the lock sent from the next)
			const int Id0 = R.St.Fed.AnchorId, Piece = R.St.L.Piece;
			Vec Middle;
			MembersOf(R.Ships, 2, &Middle);
			const bool bMiddle = (R.St.Fed.Anchor - Middle).Length() < 1e-9;
			R.Run(0.5);
			const int Id1 = R.St.Fed.AnchorId;
			R.Ships[1].bShown = false; // (one lost from sight)
			R.Frame(1.0 / 60.0);
			const int Id2 = R.St.Fed.AnchorId;
			MembersOf(R.Ships, 2, &Middle);
			const bool bMiddle2 = (R.St.Fed.Anchor - Middle).Length() < 1e-9 && R.St.L.Members.size() == 2;
			R.Run(0.5);
			const int Id3 = R.St.Fed.AnchorId;
			R.Ships[1].bShown = true; // (found again)
			R.Frame(1.0 / 60.0);
			const int Id4 = R.St.Fed.AnchorId;
			R.Ships[0].Doing = Fate::Sinking;
			R.Ships[1].Doing = Fate::Limping; // (out of the line)
			R.Frame(1.0 / 60.0);
			const int Id5 = R.St.Fed.AnchorId;
			R.Ships[2].Doing = Fate::Gone;
			const SeaFreeEye::Shot Last = R.Frame(1.0 / 60.0);
			Check("C6", "a click near a division's line locks its middle (its found members' only); a new id on every change of its members (lost, found again, sinking, out of line), never otherwise; none left lets go",
				Piece == 2 && Id0 >= SeaFreeEye::DivisionIds && bMiddle && Id1 == Id0 && Id2 != Id1 && bMiddle2 && Id3 == Id2 && Id4 != Id3 && Id5 != Id4 && !R.St.L.On() && !Last.bCut,
				"ids " + std::to_string(Id0 - SeaFreeEye::DivisionIds) + " " + std::to_string(Id1 - SeaFreeEye::DivisionIds) + " " + std::to_string(Id2 - SeaFreeEye::DivisionIds) + " " +
					std::to_string(Id3 - SeaFreeEye::DivisionIds) + " " + std::to_string(Id4 - SeaFreeEye::DivisionIds) + " " + std::to_string(Id5 - SeaFreeEye::DivisionIds) + " (over 1 << 30)");
		}
		{
			// Real seconds: unlocked, the pace changes nothing of the view; it flies with the battle standing still; a lock holds at
			// pace 4, its velocity the scene's movement over real seconds.
			Rig One, Four, Still;
			for (Rig* R : { &One, &Four, &Still })
			{
				R->Ships = { MakeShip(64, 1, 0, 0, 9500.0, 9500.0, 70.0) };
				R->Ships[0].Speed = 12.0;
				R->Key(KeyName::W, true);
			}
			bool bSame = true;
			for (int K = 0; K < 240; ++K)
			{
				One.Move(1.0 / 60.0, 1.0);
				Four.Move(1.0 / 60.0, 4.0);
				const SeaFreeEye::Shot A = One.Frame(1.0 / 60.0), B = Four.Frame(1.0 / 60.0), C = Still.Frame(1.0 / 60.0);
				bSame = bSame && (A.Eye - B.Eye).Length() == 0.0 && (A.Eye - C.Eye).Length() == 0.0;
			}
			const double Flown = (Still.Last.Eye - Still.In.DirEye).Length();
			Rig L;
			L.Ships = { MakeShip(64, 1, 0, 0, 9500.0, 9500.0, 70.0) };
			L.Ships[0].Speed = 12.0;
			L.Key(KeyName::Tab, true);
			L.Key(KeyName::Tab, false);
			bool bHolds = true, bVel = true;
			double WorstVel = 0.0;
			Vec Was;
			for (int K = 0; K < 1800; ++K)
			{
				const double Dt = K % 300 == 150 ? 0.7 : 1.0 / 60.0;
				L.Move(std::min(Dt, 0.1), 4.0);
				L.Frame(Dt);
				// (the take's frame, K 0, his shot and no lock sent; the lock sent from K 1, its velocity from K 2)
				bHolds = bHolds && L.St.L.Key == 64 && L.St.Fed.bLock == (K > 0);
				if (K == 1)
				{
					bVel = bVel && !L.St.Fed.bAnchorVel;
				}
				if (K > 1)
				{
					const Vec Want = (L.St.Fed.Anchor - Was) * (1.0 / Dt);
					WorstVel = std::max(WorstVel, (L.St.Fed.AnchorVel - Want).Length());
					bVel = bVel && L.St.Fed.bAnchorVel;
				}
				Was = L.St.Fed.Anchor;
			}
			Check("C7", "unlocked, the view is the same at pace 1, pace 4 and with the battle standing still (real seconds, never times the pace), and flies while it stands still",
				bSame && Flown > 10.0, "flown " + Num(Flown, 1) + " m in 4 s with the battle still");
			Check("C7", "a lock holds at pace 4 through hitches; its velocity the scene's movement over the frame's real seconds (the battle's step held to 0.1 s)",
				bHolds && bVel && WorstVel < 1e-9, "worst " + Num(WorstVel, 12) + " m/s off");
		}
	}

	void Floors()
	{
		std::printf("The floor and the ceiling (C1, C2)\n");
		{
			// (Shift+Q takes the pivot down, to the box's floor 20 m under the sea; the wheel brings the boom in, so the
			// eye comes down onto the water)
			Rig R;
			R.In.DirEye = Vec{ 9000.0, 9000.0, 200.0 };
			R.Key(KeyName::Q, true, true);
			for (int K = 0; K < 60 * 40; ++K)
			{
				R.St.H.Wheel += K % 30 == 0 ? 1.0 : 0.0;
				R.Frame(1.0 / 60.0);
			}
			const double Z = R.Last.Eye.Z;
			Check("C1", "held down and zoomed in over today's still face, the eye settles 3 m over it", R.Last.bFree && std::abs(Z - AgreedClear) <= 0.15, Num(Z, 3) + " m");
		}
		{
			Rig R;
			MovingSea Sea;
			R.Floor.Drawn = &MovingSeaAt;
			R.Floor.Context = &Sea;
			R.In.DirEye = Vec{ 9000.0, 9000.0, 200.0 };
			R.Key(KeyName::Q, true, true);
			double Lowest = 1e9, LowestZ = 1e9, Sum = 0.0;
			int N = 0;
			for (int K = 0; K < 60 * 60; ++K)
			{
				Sea.T += 1.0 / 60.0;
				R.St.H.Wheel += K % 30 == 0 && K < 60 * 40 ? 1.0 : 0.0;
				R.Frame(1.0 / 60.0);
				const double Clear = R.Last.Eye.Z - std::max(MovingSeaAt(&Sea, R.Last.Eye.X, R.Last.Eye.Y), 0.0);
				Lowest = std::min(Lowest, Clear);
				LowestZ = std::min(LowestZ, R.Last.Eye.Z);
				if (K >= 50 * 60)
				{
					Sum += R.Last.Eye.Z - std::max(MovingSeaAt(&Sea, R.Last.Eye.X, R.Last.Eye.Y), 0.0);
					++N;
				}
			}
			Check("C1", "held down and zoomed in over a moving 12 m sea, never under 0.5 m over it nor under its still face's 0 + 0.5 (troughs held at 0), and settling near 3 m over it",
				Lowest >= HardClear - 1e-6 && LowestZ >= HardClear - 1e-6 && Sum / N <= 3.0 * AgreedClear,
				"lowest " + Num(Lowest, 3) + " m over the sea, " + Num(LowestZ, 3) + " m over 0, " + Num(Sum / N, 2) + " m over it on average in the last 10 s");
		}
		std::string Said;
		bool bOk = true;
		for (const double Base : { -1.0, 1000.0, 400.0 })
		{
			Rig R;
			const bool bDeck = Base > 0.0;
			R.In.CeilingNow = SeaFreeEye::Ceiling(bDeck, Base);
			R.Key(KeyName::E, true, true);
			double Top = -1e9;
			for (int K = 0; K < 60 * 40; ++K)
			{
				R.St.H.Wheel -= K % 30 == 0 ? 1.0 : 0.0;
				R.Frame(1.0 / 60.0);
				Top = std::max(Top, R.Last.Eye.Z);
			}
			const double Want = CeilingOf(bDeck, Base);
			bOk = bOk && Top <= Want + 0.01 && Top >= Want - 1.0;
			Said += (Said.empty() ? "" : "; ") + std::string(bDeck ? "a deck at " + Num(Base, 0) + " m: " : "no deck: ") + Num(Top, 2) + " m of " + Num(Want, 0);
		}
		Check("C2", "held up and zoomed out, the eye reaches the ceiling and never passes it: 1,500 m, or under a deck min(1500, max(300, base - 150)) (Q7)", bOk, Said);
		// Shift+E held 30 real seconds against the ceiling, with no wheel and with the wheel turned out 20 notches as the tour
		// turns it, at 24, 60 and 144 frames a second: the same boom at the end within 2 per cent at every rate, the eye at
		// the ceiling and never over it (the core's own ceiling takes a held rise back by shortening the boom at a speed set
		// by the frame rate, core ask 13: 1 m at 24, 58 m at 60 and 249 m at 144 after 30 s, so the glue feeds Rise no
		// further than the room left under the ceiling).
		std::string Rates;
		bool bRates = true;
		for (const double Out : { 0.0, 20.0 })
		{
			double Least = 1e18, Most = -1e18;
			Rates += Rates.empty() ? "" : "; ";
			Rates += Out == 0.0 ? "no wheel:" : "the wheel out:";
			for (const double Hz : { 24.0, 60.0, 144.0 })
			{
				Rig R;
				R.In.DirEye = Vec{ 9570.0, 8205.0, 300.0 };
				R.In.DirLook = Vec{ 10070.0, 8705.0, 0.0 };
				R.Key(KeyName::E, true, true);
				double Top = -1e9;
				const int Frames = static_cast<int>(std::lround(30.0 * Hz));
				for (int K = 0; K < Frames; ++K)
				{
					R.St.H.Wheel -= Out / static_cast<double>(Frames);
					R.Frame(1.0 / Hz);
					Top = std::max(Top, R.Last.Eye.Z);
				}
				bRates = bRates && R.Last.bFree && Top <= AgreedCeiling + 0.01 && R.Last.Eye.Z >= AgreedCeiling - 5.0;
				Least = std::min(Least, R.Free.Arm());
				Most = std::max(Most, R.Free.Arm());
				Rates += " " + Num(R.Free.Arm(), 1) + " m at " + Num(Hz, 0);
			}
			bRates = bRates && Most <= 1.02 * Least;
		}
		Check("C2", "Shift+E held 30 s against the ceiling, with and without the wheel turned out, ends with the same boom within 2 per cent at 24, 60 and 144 frames a second, the eye at the ceiling and never over it",
			bRates, Rates);
	}

	void Rules()
	{
		std::printf("Who owns the camera, the cut, the refreshes (C17, C20)\n");
		int Owner = 0, Cut = 0;
		for (int M = 0; M < 8; ++M)
		{
			const bool bS = M & 1, bT = M & 2, bH = M & 4;
			Owner += SeaFreeEye::EyeOwner(bS, bT, bH) == (bS && bT && !bH) ? 1 : 0;
		}
		for (int M = 0; M < 128; ++M)
		{
			const bool bFree = M & 1, bEye = M & 2, bPose = M & 4, bShot = M & 8, bMoved = M & 16, bFace = M & 32, bWindow = M & 64;
			const bool bToday = bEye || (bShot && bMoved) || bFace || (bWindow && bMoved);
			const bool bWant = bFree ? (bEye || bPose) : bToday;
			Cut += SeaFreeEye::CutNow(bFree, bEye, bPose, bShot, bMoved, bFace, bWindow) == bWant ? 1 : 0;
		}
		Check("C17", "EyeOwner over its 8 rows (the switch, the camera taken, no dive of his); CutNow over its 128: not free, today's expression on every row; free, his owed cut or the core's and nothing of the director's",
			Owner == 8 && Cut == 128, std::to_string(Owner) + " of 8, " + std::to_string(Cut) + " of 128");
		// The refreshes, free, with the battle standing still: by real seconds and by distance, at a cut and a new hero.
		SeaFreeEye::Refresh Fun, Far;
		int FunDue = 0, FarDue = 0, FunMoved = 0, FarMoved = 0;
		for (int K = 0; K <= 600; ++K)
		{
			const double T = K / 60.0;
			FunDue += SeaFreeEye::FunnelsDue(Fun, T, Vec{ 100.0, 100.0, 50.0 }, false, 7) ? 1 : 0;
			FarDue += SeaFreeEye::FarWreckDue(Far, T, Vec{ 100.0, 100.0, 50.0 }, false, 175.0) ? 1 : 0;
		}
		SeaFreeEye::Refresh Fun2, Far2;
		for (int K = 0; K < 60; ++K)
		{
			const double T = K / 60.0;
			// (3 km a second, a fast pan at the top: 50 m a frame, so the funnels at frames 0, 20 and 40, the first 1,000 m on
			// each, 3; the far wrecks at every fourth frame from 0 to 56, the first 200 m past 175 on each, 15)
			const Vec At{ 100.0 + 3000.0 * T, 100.0, 50.0 };
			FunMoved += SeaFreeEye::FunnelsDue(Fun2, T * 0.01, At, false, 7) ? 1 : 0;
			FarMoved += SeaFreeEye::FarWreckDue(Far2, T * 0.01, At, false, 175.0) ? 1 : 0;
		}
		const bool bCut = SeaFreeEye::FunnelsDue(Fun, 10.01, Vec{ 100.0, 100.0, 50.0 }, true, 7) && SeaFreeEye::FarWreckDue(Far, 10.01, Vec{ 100.0, 100.0, 50.0 }, true, 175.0);
		const bool bHero = SeaFreeEye::FunnelsDue(Fun, 10.02, Vec{ 100.0, 100.0, 50.0 }, false, 8);
		const bool bQuiet = !SeaFreeEye::FunnelsDue(Fun, 10.03, Vec{ 100.0, 100.0, 50.0 }, false, 8);
		Check("C20", "free, with the battle standing still the funnels come every 4 real seconds and every 1 km moved, the far wrecks every half a real second and every 175 m (half LiveReach); each at a cut, the funnels at a new hero, and nothing else",
			FunDue == 3 && FarDue == 21 && FunMoved == 3 && FarMoved == 15 && bCut && bHero && bQuiet,
			"funnels " + std::to_string(FunDue) + " in 10 s, " + std::to_string(FunMoved) + " over 3 km; far wrecks " + std::to_string(FarDue) + " in 10 s, " + std::to_string(FarMoved) + " over 3 km");
	}

	/** The tour played at a steady rate, or at 60 with drive seconds [Slow, Slow + 1) at 30; each step's mean and worst
	 * whole second as its log line says them (the Mac's gate reads them), and the number of step lines. */
	struct StepRate
	{
		std::string Mean, Worst;
	};
	std::vector<StepRate> TourRates(double Hz, double Slow, int& Lines)
	{
		Rig R;
		std::string Error;
		SeaFreeEye::ParseDrive(SeaFreeEye::TourText(), R.St.D, Error);
		std::vector<StepRate> Out;
		Lines = 0;
		for (int K = 0; K < 200 * 144 && !R.St.D.bDone; ++K)
		{
			const double T = R.St.D.T;
			R.Frame(Slow >= 0.0 && T >= Slow - 1e-9 && T < Slow + 1.0 - 1e-9 ? 1.0 / 30.0 : 1.0 / Hz);
			for (const std::string& Line : SeaFreeEye::TakeSaid(R.St))
			{
				const size_t Over = Line.find(" a second over the step, its worst whole second "), Frames = Line.find(" frames, ");
				if (Line.rfind("Sea free eye: drive step ", 0) != 0 || Over == std::string::npos || Frames == std::string::npos || Frames > Over)
				{
					continue;
				}
				++Lines;
				const size_t WorstAt = Over + std::strlen(" a second over the step, its worst whole second ");
				Out.push_back(StepRate{ Line.substr(Frames + 9, Over - Frames - 9), Line.substr(WorstAt, Line.size() - WorstAt - 1) });
			}
		}
		return Out;
	}

	void DriveLog()
	{
		std::printf("The drive's log lines (C15, the Mac's gate)\n");
		// At a steady rate every step's line says that rate: over the step and in its worst whole second (a frame landing a
		// hair either side of a whole second on the summed clock never moves into its neighbour; the plan's gate is every
		// step at 45 or better, so a Mac holding exactly 45 must read 45).
		std::string Said;
		bool bSteady = true;
		for (const int Hz : { 24, 30, 45, 60, 144 })
		{
			int Lines = 0;
			const std::vector<StepRate> Steps = TourRates(static_cast<double>(Hz), -1.0, Lines);
			int Right = 0;
			for (const StepRate& One : Steps)
			{
				Right += One.Mean == std::to_string(Hz) + ".0" && One.Worst == std::to_string(Hz) ? 1 : 0;
			}
			bSteady = bSteady && Lines == 11 && Right == Lines;
			Said += (Said.empty() ? "" : ", ") + std::to_string(Right) + " of " + std::to_string(Lines) + " at " + std::to_string(Hz);
		}
		Check("C15", "the tour at a steady 24, 30, 45, 60 and 144 frames a second: each of its 11 step lines says that rate over the step and in its worst whole second",
			bSteady, Said);
		// At 60 with one whole second of the fourth step, the first Tab's 12.5 to 30 s (drive seconds 19.5 to 20.5, its
		// eighth whole second), at 30: that step's worst whole second is 30, every other step's 60.
		int Lines = 0;
		const std::vector<StepRate> Steps = TourRates(60.0, 19.5, Lines);
		std::string Worsts;
		bool bSlow = Lines == 11 && Steps.size() == 11;
		for (size_t K = 0; K < Steps.size(); ++K)
		{
			bSlow = bSlow && Steps[K].Worst == (K == 3 ? "30" : "60");
			Worsts += (Worsts.empty() ? "" : " ") + Steps[K].Worst;
		}
		Check("C15", "the tour at 60 with one whole second of its fourth step at 30: that step's worst whole second 30, every other step's 60", bSlow, "worst seconds " + Worsts);
	}
}

int main(int ArgC, char** ArgV)
{
	if (ArgC < 3)
	{
		std::printf("usage: sea_free_eye_test <Source/TheaterCommand> <director.sha> [--quick] recording.json ...\n");
		return 2;
	}
	const std::string Src = ArgV[1], ShaPath = ArgV[2];
	bool bQuick = false;
	std::vector<std::string> Paths;
	for (int A = 3; A < ArgC; ++A)
	{
		const std::string Arg = ArgV[A];
		if (Arg == "--quick")
		{
			bQuick = true;
		}
		else
		{
			Paths.push_back(Arg);
		}
	}
	std::printf("The sea free eye's cloud test%s\n", bQuick ? " (quick: each recording's first minutes)" : "");
	SceneText(Src, ShaPath);
	Keys();
	Switches();
	Lens();
	Takes();
	Picks();
	Floors();
	Rules();
	DriveLog();

	std::printf("The recordings, played as the scene plays them\n");
	Book B;
	bool bGaleDone = false;
	int ToursRun = 0, DeckTours = 0;
	std::string TourSaid, HiddenSaid, Each;
	for (const std::string& Path : Paths)
	{
		Played P;
		if (!Load(Path, P))
		{
			return 2;
		}
		const double Fight = P.EndT - P.BeginT;
		const double Most = bQuick ? 120.0 : 0.0;
		std::vector<RunSpec> Runs;
		RunSpec Hands60;
		Hands60.Name = "his hands, 60 a second, pace 1, a moving 12 m sea";
		Hands60.Floor = 1;
		Hands60.RealMost = Most;
		Hands60.Seed = 11;
		Runs.push_back(Hands60);
		RunSpec Fast = Hands60;
		Fast.Name = "24 a second, pace 4";
		Fast.Frames = Timing::TwentyFour;
		Fast.Pace = 4.0;
		Fast.Seed = 12;
		Fast.RealMost = bQuick ? 60.0 : 0.0;
		Runs.push_back(Fast);
		RunSpec Slow;
		Slow.Name = "hitches, pace a quarter, then standing still";
		Slow.Frames = Timing::Hitches;
		Slow.Pace = 0.25;
		Slow.RealMost = bQuick ? 60.0 : 300.0;
		Slow.StillFrom = bQuick ? 30.0 : 200.0;
		Slow.StillFor = bQuick ? 20.0 : 60.0;
		Slow.Seed = 13;
		Runs.push_back(Slow);
		RunSpec Jitter;
		Jitter.Name = "jittered 8 to 33 ms, under a deck at 400 m, the window's shape changed";
		Jitter.Frames = Timing::Jitter;
		Jitter.Floor = 1;
		Jitter.bDeck = true;
		Jitter.DeckBase = 400.0;
		Jitter.RealMost = bQuick ? 60.0 : 300.0;
		Jitter.ShapeAt = bQuick ? 20.0 : 100.0;
		Jitter.Seed = 14;
		Runs.push_back(Jitter);
		long Frames = 0, FreeFrames = 0;
		double Lowest = 1e9, Highest = -1e9;
		for (const RunSpec& R : Runs)
		{
			const RunOut O = Play(P, R, B);
			Frames += O.Frames;
			FreeFrames += O.FreeFrames;
			Lowest = std::min(Lowest, O.LowestClear);
			Highest = std::max(Highest, O.Highest);
			if (&R == &Runs.front())
			{
				// C16: the same hands with every hidden ship deleted from the show's list: bit for bit the same.
				RunSpec Gone = R;
				Gone.bHiddenGone = true;
				const RunOut G = Play(P, Gone, B);
				const bool bSame = G.Digest == O.Digest && G.Said == O.Said && G.Frames == O.Frames;
				if (!bSame)
				{
					B.Fail("C16", P.Name + ": the free eye differs with the hidden ships deleted");
				}
				B.Count(P.HiddenSteps > 0 ? "recordings with hidden ships played twice" : "recordings with no hidden ship played twice");
				HiddenSaid += (HiddenSaid.empty() ? "" : ", ") + P.Name + " " + std::to_string(P.HiddenSteps) + (bSame ? " same" : " DIFFERENT");
			}
		}
		// C15: the tour, on a recording with three found ships of his own and a fight long enough from there.
		double Start = -1.0;
		{
			std::vector<ShipOut> Now;
			for (size_t K = 0; K < P.Snaps.size() && Start < 0.0; K += 4)
			{
				const double T = P.BeginT + static_cast<double>(K) * SnapStep;
				ShipsAt(P, T, Now, true);
				int Own = 0;
				for (const ShipOut& Ship : Now)
				{
					Own += Pickable(Ship) && Ship.Side == P.Mine ? 1 : 0;
				}
				Start = Own >= 3 && P.EndT - T >= 96.0 ? T : -1.0;
			}
		}
		std::string TourOne = "no tour (fewer than 3 found ships of his own, or under 96 s of fight after)";
		if (Start >= 0.0)
		{
			RunSpec Tour;
			Tour.Name = "the tour";
			Tour.bTour = true;
			Tour.StartT = Start;
			const RunOut O = Play(P, Tour, B);
			++ToursRun;
			TourOne = "the tour from " + Num(Start - P.BeginT, 0) + " s: " + O.TourWords;
			if (TourSaid.empty())
			{
				for (const std::string& Line : O.Said)
				{
					TourSaid += "    " + Line + "\n";
				}
			}
			if (!O.bTourOk)
			{
				B.Fail("C15", P.Name + ": " + O.TourWords);
			}
			if (DeckTours == 0)
			{
				for (const double Base : { 1000.0, 400.0 })
				{
					RunSpec Under = Tour;
					Under.bDeck = true;
					Under.DeckBase = Base;
					const RunOut U = Play(P, Under, B);
					++DeckTours;
					TourOne += "; under a deck at " + Num(Base, 0) + " m up to " + Num(U.Highest, 1) + " m";
					if (!U.bTourOk)
					{
						B.Fail("C15", P.Name + " under a deck at " + Num(Base, 0) + " m: " + U.TourWords);
					}
				}
			}
		}
		// C1: the real gale, on the first recording, low over it.
		if (!bGaleDone)
		{
			RunSpec Gale;
			Gale.Name = "the real gale";
			Gale.Floor = 2;
			Gale.Frames = Timing::TwentyFour;
			Gale.RealMost = bQuick ? 8.0 : 40.0;
			Gale.Seed = 15;
			const RunOut O = Play(P, Gale, B);
			B.Count("gale: lowest clearance over SampleNear, mm", static_cast<long>(std::lround(1000.0 * O.LowestClear)));
			bGaleDone = true;
		}
		std::printf("  %s: %.0f s of fight, %d ships at the most, %d quarter seconds with a ship not found; %ld frames played, %ld free; the eye %.2f m over the sea at the lowest, %.1f m at the highest; %s\n",
			P.Name.c_str(), Fight, P.MostShips, P.HiddenSteps, Frames, FreeFrames, Lowest, Highest, TourOne.c_str());
	}
	if (!TourSaid.empty())
	{
		std::printf("  the tour's log lines on the first recording it ran on (the Mac reads its gate from these):\n%s", TourSaid.c_str());
	}
	auto Bad = [&B](const char* Id) { return B.Bad.count(Id) ? B.Bad.at(Id) : 0L; };
	auto Why = [&B](const char* Id) { return B.First.count(Id) ? "; first: " + B.First.at(Id) : std::string(); };
	auto Seen = [&B](const char* What) { return B.Seen.count(What) ? B.Seen.at(What) : 0L; };
	const bool bPlayed = !Paths.empty();
	Check("C1", "every free frame of every run, the eye at least 0.5 m over the drawn sea and over its still face's 0 (the moving 12 m sea, the real gale against its exact drawn surface by 0.45 m)",
		bPlayed && Bad("C1") == 0 && Seen("gale frames") > 0,
		std::to_string(Seen("gale frames")) + " gale frames, crests to " + Num(Seen("gale: crest under the eye, cm") / 100.0, 2) + " m under the eye" + Why("C1"));
	Check("C2", "every free frame, the eye never over the ceiling (1,500 m; 300 m under a deck at 400 m) and the pivot in its box (the board and 3 km, -20 m to the ceiling; over the top only by the floor's lift)",
		bPlayed && Bad("C2") == 0,
		std::to_string(Bad("C2")) + " frames out; " + std::to_string(Seen("frames with the pivot over the box's top under an eye the floor holds")) + " frames with the pivot over the top under an eye the floor holds, by " +
			Num(Seen("the pivot over the box's top under an eye the floor holds, cm") / 100.0, 2) + " m at the most" + Why("C2"));
	Check("C3", "every lock, pick and division on found ships afloat; no click or ray straight at a hidden boat ever reaching her", bPlayed && Bad("C3") == 0 && Seen("rays through hidden boats") > 0,
		std::to_string(Seen("clicks straight at hidden boats")) + " clicks and " + std::to_string(Seen("rays through hidden boats")) + " rays at hidden boats" + Why("C3"));
	Check("C4", "a lock on a ship lost from sight, sinking or gone let go that frame, never sent, never a cut", bPlayed && Bad("C4") == 0,
		std::to_string(Seen("let goes on a ship lost, sinking or gone")) + " let goes" + Why("C4"));
	Check("C5", "the Tab order against its recount through every recording", bPlayed && Bad("C5") == 0 && Seen("Tab orders recounted") > 0,
		std::to_string(Seen("Tab orders recounted")) + " recounted" + Why("C5"));
	Check("C6", "a division's members its found ships in line, its middle theirs, its id renewed at every change of them and never otherwise", bPlayed && Bad("C6") == 0,
		std::to_string(Seen("division frames")) + " division frames, " + std::to_string(Seen("division member changes")) + " changes" + Why("C6"));
	Check("C7", "real seconds: a lock's velocity the scene's movement over real seconds (none on its first frame), a lock never lost with its ship afloat and found, at paces a quarter, 1 and 4",
		bPlayed && Bad("C7") == 0 && Seen("velocities measured") > 0 && Seen("frames flown while the battle stands still") > 0,
		std::to_string(Seen("velocities measured")) + " velocities, " + std::to_string(Seen("frames flown while the battle stands still")) + " frames flown with the battle still" + Why("C7"));
	Check("C12", "the lens never faster than 40 degrees a real second between cuts, and a take never changing the picture's width", bPlayed && Bad("C12") == 0, std::to_string(Bad("C12")) + " frames" + Why("C12"));
	Check("C13", "every take the director's shot within 1 cm (or lifted clear, a cut)", bPlayed && Bad("C13") == 0 && Seen("takes") > 0,
		std::to_string(Seen("takes")) + " takes, " + std::to_string(Seen("takes lifted out of the water or let down under the ceiling (a cut)")) + " of them cuts, " +
			std::to_string(Seen("takes let down from over the ceiling")) + " let down from over the ceiling" + Why("C13"));
	Check("C14", "U: given back first, the director's through his dive; a take from under the water a cut, the eye clear", bPlayed && Bad("C14") == 0 && Seen("dives") > 0,
		std::to_string(Seen("dives")) + " dives" + Why("C14"));
	Check("C15", "the tour on every recording with three found ships of his own and the fight for it: each step at its time, the eye to the ceiling (and a deck's cap), the camera given back",
		bPlayed && Bad("C15") == 0 && ToursRun > 0 && DeckTours == 2, std::to_string(ToursRun) + " recordings toured" + Why("C15"));
	Check("C16", "every recording played twice with the same hands, once with every hidden ship deleted: poses, picks, Tab orders, ids, cuts and log lines bit for bit the same",
		bPlayed && Bad("C16") == 0 && Seen("recordings with hidden ships played twice") > 0, HiddenSaid);
	Check("C19", "no lock, pick or camera kept through a give back; no take while he had the camera", bPlayed && Bad("C19") == 0, std::to_string(Seen("give backs")) + " give backs" + Why("C19"));
	std::printf("sea_free_eye_test: %d checks, %d failed\n", Checks, Failed);
	return Failed == 0 ? 0 : 1;
}
