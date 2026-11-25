#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

/**
 * What stands on the ground under a raid on troops, in plain C++ (Brian, 24 November 2026: "i want every air battle
 * playable" and "if a bomber group is bombing troops instead of a factory or town and they get intercepted than i want
 * that playable"). The same code runs in the game (AirShow lands the bombs and flies the strafing passes by it,
 * AAirBattle draws the pieces it keeps) and in the cloud, where Tools/air_show plays a marked recording through it.
 *
 * The rules decide everything that counts (the every-kind contract, /mnt/project-files/air-battles/every-kind/
 * CONTRACT.md, sections 2 to 4, with the air rules thread's additions in raid-troops-ticket.md): which pieces stand
 * where (place.marks: at most 64, placed by the rules inside the target ring), which bomb or burst struck which piece
 * and how (air:mark-hit), what each piece is now (view.marks: ok, hit, burn, out; the men left in a block), and the
 * organisation the raid cut (view.marks.cut, 0 to 0.04: a raid is capped at 4% of organisation and 2% of strength, so
 * most hits are near misses and men scattered, and a few vehicles wrecked). Nothing here changes any of it. What is
 * decided here is only what the rules leave open: where each drawn bomb of a stick comes down (a counted hit on its
 * piece, every other bomb clear of every piece: "most shots miss, but every hit shows on what it struck"), the lane's
 * geometry a strafing pass flies down, and where the column's light flak lays its streams (short of the lead).
 *
 * Places are board metres (x east, y north), headings radians clockwise from north, as the feed's. Nothing here draws
 * on any dice of the show's: every choice is a hash of the pieces' ids and a salt, so the flight plays the same.
 */
namespace AirMarks
{
	/** A piece as the rules have it now (view.marks: ok, hit (damaged, smoking), burn, out (wrecked)). Ordered: a hit
	 * never takes a piece down the list, and a piece that is out never changes again. */
	enum class State : int { Ok = 0, Hit = 1, Burn = 2, Out = 3 };
	State StateOf(const std::string& Word);
	const char* WordOf(State S);

	/** One piece on the ground (place.marks.pieces). */
	struct Piece
	{
		int Id = 0;
		/** tank, lorry, halftrack, car, wagon, gun (a towed field gun), aa (a flak gun), men (a block of N men); the
		 * fieldworks' words of the contract's step 2 are kept as given and drawn as nothing yet. */
		std::string What, Model, Nat, Posture;
		double X = 0.0, Y = 0.0, Face = 0.0;
		/** A block of men: how many; anything else 1. */
		int N = 1;
		/** Its footprint, metres: along its face and across (SizeOf). */
		double Long = 6.0, Wide = 2.5;
		/** As the scene shows it now: its state and the men left in a block (N at first). */
		State Now = State::Ok;
		int Men = 1;
	};

	/** A view's word on a piece (view.marks.pieces: only those changed since the last view). Men -1: not said. */
	struct Change
	{
		int Id = 0;
		State To = State::Ok;
		int Men = -1;
	};

	/** view.marks: the share of the column's guns manned and firing back (fire), the organisation cut so far (cut), and
	 * the pieces changed. bGiven false: the view carried no marks. */
	struct View
	{
		bool bGiven = false;
		double Fire = 1.0, Cut = 0.0;
		std::vector<Change> Pieces;
	};

	/** A corps under the raid (place.marks.corps, the air rules thread's addition): its words, for the screen only. */
	struct Corps
	{
		int Id = 0;
		std::string Nat, Name, Kind, Posture;
		int Divs = 0, Men = 0;
		double Str = 1.0, Org = 1.0;
	};

	/** place.marks: whose they are, how they stand, the road a march column is on, what stands in the province (words
	 * only), the men one drawn figure stands for, and the pieces. */
	struct Column
	{
		std::string Nat, Posture;
		std::vector<std::pair<double, double>> Road;
		int Divs = 0, Men = 0, Per = 10;
		std::vector<Piece> Pieces;
		/** place.marks.corps: the corps under the raid. */
		std::vector<Corps> Under;
		/** The counts the ticket sent instead of pieces ({ tank: 6, lorry: 20, gun: 8, aa: 4, men: 600 }), in the order
		 * given; Lay turns them into pieces. bLaid: the pieces were laid here, not by the rules. */
		std::vector<std::pair<std::string, int>> Counts;
		bool bLaid = false;
		bool Has() const { return !Pieces.empty(); }
		/** The piece's place in Pieces, -1 for none. */
		int IndexOf(int Id) const;
	};

	/** The most pieces the rules place (raid-troops-ticket.md). */
	constexpr int MostPieces = 64;

	/** A piece's footprint by what it is, metres along its face and across. */
	void SizeOf(const std::string& What, double& Long, double& Wide);
	/** Lays the pieces from the counts when the ticket sent only counts (the contract, section 2): along the road given,
	 * or one laid through the aim from the seed, a march column's spacing between them; any other posture in a loose
	 * square about the aim. All inside R of the aim; at most MostPieces. The same seed lays the same column. */
	void Lay(Column& C, double AimX, double AimY, double R, uint64_t Seed);

	/** How far a board point is from a piece's footprint, metres (0 inside it). */
	double Off(const Piece& P, double X, double Y);
	/** A bomb's blast as the scene keeps it clear of what it did not strike, metres from a footprint, by its size (kg). */
	double BlastOf(double Kg);
	/** No other piece within Blast of the point (Skip: a piece's place in Pieces left out, -1 none). */
	bool Clear(const Column& C, double X, double Y, double Blast, int Skip = -1);
	/** The nearest point to (X, Y) clear of every piece by more than Blast; the point itself when it is. False where
	 * none lies within 800 m. */
	bool ClearOf(const Column& C, double X, double Y, double Blast, double& OutX, double& OutY);
	/** Where a counted hit's drawn bomb comes down on the piece at Index: inside its footprint for a wreck, a burn or a
	 * blast; beside it, within its blast, for a near miss or men scattered; clear of every other piece by more than
	 * Blast either way, where any such spot is (else the spot tried farthest from them, still on it or beside it as
	 * the hit says). Salt makes each bomb's spot its own. */
	void AimOn(const Column& C, int Index, const std::string& How, double Blast, uint32_t Salt, double& OutX, double& OutY);
	/** What a hit makes of a piece as it is shown (the event's how; never down the list): wreck and blast struck, burn
	 * burning; a near miss and men scattered as they were. Out only by a view's word (view.marks), or where the caller
	 * finds a block of men with none left. */
	State StateAfter(const std::string& How, State Now);
	/** A piece takes a state and the men left (-1: as they were): false where nothing changed. A piece that is out
	 * never changes again. */
	bool Apply(Piece& P, State To, int Men);

	/** A strafing pass's lane (air:strafe's extra: from, to, width), as land:strafe's in the land battle. */
	struct Lane
	{
		double FromX = 0.0, FromY = 0.0, ToX = 0.0, ToY = 0.0, Width = 60.0;
		double Long() const;
		/** Its heading, from its start to its end. */
		double Way() const;
		/** A board point as metres along the lane from its start (U) and across it, right of its way plus (V). */
		void Along(double X, double Y, double& U, double& V) const;
		void At(double U, double V, double& X, double& Y) const;
	};
	/** The lane a pass flies when the event gives none: down the road through the aim, 1,200 m of it (the column's
	 * own pieces' middle where there is no road). */
	Lane LaneOf(const Column& C, double AimX, double AimY);
	/** The pieces whose footprints the lane covers, in their order along it (places in Pieces). */
	std::vector<int> InLane(const Column& C, const Lane& L);

	/**
	 * How a machine flies its strafing pass: LandAir's machines' figures (Battle/LandAir.cpp MachineOf, the art
	 * director's notes of 23 November 19:08Z): a fighter from 600 to 1,000 m over the ground in a 20 to 30 degree dive,
	 * its guns from about 900 m to 250 m, out at the bottom 30 to 60 m up at 4 g and away in a 25 degree climb; a Stuka
	 * from high in a 75 degree dive; an Il-2 low and shallow, out at 15 m. One behind another, Gap seconds apart.
	 */
	struct Pass
	{
		double High = 800.0, Low = 45.0, Dive = 25.0, Climb = 25.0;
		double Open = 900.0, Shut = 250.0, PullG = 4.0, Speed = 130.0, Gap = 6.0;
	};
	Pass PassOf(const std::string& Model);
	/** The height a pull-out at PullG from a dive of Gam radians (down: negative) at Speed takes to come level, metres. */
	double PullRoom(double Speed, double Gam, double PullG);

	/**
	 * The column's light flak firing back (drawn only; it never decides a loss): each gun's bursts, as LandAir lays a
	 * ground gun's (Sky::Build's posts): 1 to 2.4 s on, 0.5 to 1.5 s off, its stream across the aircraft by Across
	 * metres, Up over it, and short of its lead by Lead (0.2 to 0.7 of the full lead), so that the tracers fall behind.
	 * The same gun bursts the same at the same air time.
	 */
	struct Burst
	{
		double On = 0.0, Off = 0.0, Across = 0.0, Up = 0.0, Lead = 0.5;
	};
	bool BurstAt(int PieceId, double AirT, Burst& Out);
	/** The light flak's reach (metres, slant) and its tracer's speed (metres a second, as LandAir's TracerMps). */
	constexpr double FlakReach = 1800.0, TracerMps = 850.0;

	/** A whole number from two (every choice here). */
	uint32_t Hash(uint32_t A, uint32_t B);
	/** [0, 1) from a hash. */
	double UnitOf(uint32_t H);
}
