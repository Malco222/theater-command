// A fleet under way on the campaign map, its cloud test (Tools/ship_way/run.sh): the game's own Campaign/ShipWay.h, no
// engine. Given a file name it also writes the scenes it checked (the land, the way the map found, the course she
// sails, and where she is at each tenth of her time) as JSON for Tools/ship_way/picture.py.

#include "Campaign/ShipWay.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

using namespace TheaterShipWay;

namespace
{
	int Checks = 0, Faults = 0;

	void Check(bool bOk, const std::string& What)
	{
		++Checks;
		if (!bOk)
		{
			++Faults;
			std::printf("FAULT: %s\n", What.c_str());
		}
	}

	bool Near(double A, double B, double Slack) { return std::fabs(A - B) <= Slack; }

	/** Land, for a test: round islands, and everything south of a line. */
	struct FLand
	{
		struct FIsle { FSpot Mid; double Radius; };
		std::vector<FIsle> Isles;
		bool bShore = false; // land wherever Y is under ShoreY
		double ShoreY = 0.0;
		bool Wet(FSpot P, double Eroded = 0.0) const
		{
			if (bShore && P.Y < ShoreY - Eroded)
			{
				return false;
			}
			for (const FIsle& Isle : Isles)
			{
				if (Size(P - Isle.Mid) < Isle.Radius - Eroded)
				{
					return false;
				}
			}
			return true;
		}
		double Off(FSpot P) const // how far from the nearest land (under nought: ashore)
		{
			double Least = bShore ? P.Y - ShoreY : 1.0e9;
			for (const FIsle& Isle : Isles)
			{
				const double D = Size(P - Isle.Mid) - Isle.Radius;
				Least = D < Least ? D : Least;
			}
			return Least;
		}
	};

	FCourse PlanOn(const FLand& Land, const FAsk& Ask)
	{
		return Plan(Ask, [&Land](FSpot P) { return Land.Wet(P); });
	}

	/** The way come from point to point of a course, as its points lie (a turn made where she lies counts nothing here). */
	std::vector<double> MarksOf(const FCourse& Course)
	{
		std::vector<double> Marks(Course.Points.size(), 0.0);
		for (size_t K = 1; K < Course.Points.size(); ++K)
		{
			Marks[K] = Marks[K - 1] + Size(Course.Points[K] - Course.Points[K - 1]);
		}
		return Marks;
	}

	/** The greatest change of heading between one point of a course and the next, in radians, and (over points apart) how far a chord lies off the headings at its ends. */
	double Jump(const FCourse& Course)
	{
		double Most = 0.0;
		for (size_t K = 1; K < Course.Points.size(); ++K)
		{
			const double Turn = std::fabs(std::atan2(Cross(Course.Heads[K - 1], Course.Heads[K]), Dot(Course.Heads[K - 1], Course.Heads[K])));
			Most = Turn > Most ? Turn : Most;
			const FSpot Chord = Course.Points[K] - Course.Points[K - 1];
			if (Size(Chord) > 1.0e-6)
			{
				const FSpot Way = Unit(Chord, Course.Heads[K]);
				for (const FSpot& Head : { Course.Heads[K - 1], Course.Heads[K] })
				{
					const double Off = std::fabs(std::atan2(Cross(Way, Head), Dot(Way, Head)));
					Most = Off > Most ? Off : Most;
				}
			}
		}
		return Most;
	}

	/** The least distance off the land along a course, asked every Each of the way (the ends left out, as the game leaves them). */
	double LeastOff(const FLand& Land, const FCourse& Course, double Each, double EndSlack)
	{
		double Least = 1.0e9;
		for (size_t K = 1; K < Course.Points.size(); ++K)
		{
			const FSpot A = Course.Points[K - 1], B = Course.Points[K];
			const int Asks = static_cast<int>(std::ceil(Size(B - A) / Each)) + 1;
			for (int At = 0; At <= Asks; ++At)
			{
				const FSpot P = A + (B - A) * (static_cast<double>(At) / Asks);
				if (Size(P - Course.Points.front()) <= EndSlack || Size(P - Course.Points.back()) <= EndSlack)
				{
					continue;
				}
				const double Off = Land.Off(P);
				Least = Off < Least ? Off : Least;
			}
		}
		return Least;
	}

	double MarksOfWay(const std::vector<FSpot>& Way)
	{
		double Long = 0.0;
		for (size_t K = 1; K < Way.size(); ++K)
		{
			Long += Size(Way[K] - Way[K - 1]);
		}
		return Long;
	}

	double LeastOffWay(const FLand& Land, const std::vector<FSpot>& Way, double Each)
	{
		FCourse AsCourse;
		AsCourse.Points = Way;
		return LeastOff(Land, AsCourse, Each, 0.0);
	}

	const double Helm = 2.4; // the fastest her head may swing, in radians a second, as the game asks it

	/** All the turning of a course, added up, in radians. */
	double Turned(const FCourse& Course)
	{
		double Sum = 0.0;
		for (size_t K = 1; K < Course.Heads.size(); ++K)
		{
			Sum += std::fabs(std::atan2(Cross(Course.Heads[K - 1], Course.Heads[K]), Dot(Course.Heads[K - 1], Course.Heads[K])));
		}
		return Sum;
	}

	/** What is wrong with her times over a course: nothing, if they are as they should be. */
	std::string TimesWrong(const FCourse& C, const FTimes& T, double Asked)
	{
		const size_t Many = T.Ran.size();
		if (Many < 2 || T.At.size() != Many || T.Head.size() != Many)
		{
			return "its tables are not of one length";
		}
		if (T.Ran.front() != 0.0 || !Near(T.Ran.back(), C.Length, 1.0e-9))
		{
			return "she does not run from nought to the course's length";
		}
		if (Size(T.At.front() - C.Points.front()) > 1.0e-9 || Size(T.At.back() - C.Points.back()) > 1.0e-9)
		{
			return "she does not begin and end where the course does";
		}
		if (Size(T.Head.front() - C.Heads.front()) > 1.0e-9 || Size(T.Head.back() - C.Heads.back()) > 1.0e-9)
		{
			return "she does not begin on her bows and end as the table wants her";
		}
		for (size_t K = 1; K < Many; ++K)
		{
			if (T.Ran[K] < T.Ran[K - 1])
			{
				return "she goes astern";
			}
			if (!Near(Size(T.Head[K]), 1.0, 1.0e-9) || !std::isfinite(T.At[K].X) || !std::isfinite(T.At[K].Y))
			{
				return "a heading is not a unit, or a place is not a number";
			}
		}
		if (!Near(T.Seconds, static_cast<double>(Many - 1) * T.Tick, 1.0e-9))
		{
			return "her time is not the time of her ticks";
		}
		if (T.Seconds < Asked)
		{
			return "she is quicker than was asked";
		}
		// (she takes the time asked where her turns let her, and never more than half as long again: past that her helm
		// is made quicker, and only then)
		if (T.Seconds > Longest * Asked + 0.2 + 2.0 * T.Tick && T.Helm < 8.0 * Helm * 0.999)
		{
			return "she takes longer than half as long again as was asked (" + std::to_string(T.Seconds) + " s for " + std::to_string(Asked) + " asked)";
		}
		if (T.Helm < Helm || T.Helm > 8.0 * Helm * 1.001 || (T.Helm > Helm * 1.001 && T.Seconds < Longest * Asked * 0.98))
		{
			return "her helm was made quicker than the time wanted of it (" + std::to_string(T.Helm) + " radians a second, " + std::to_string(T.Seconds) + " s for " + std::to_string(Asked) + " asked)";
		}
		if (T.Swing > T.Helm * 1.02 + 0.1)
		{
			return "her head swings faster than her helm allows (" + std::to_string(T.Swing) + " radians a second, helm " + std::to_string(T.Helm) + ")";
		}
		return "";
	}

	/** Every scene checked, kept for the picture. */
	struct FScene
	{
		std::string Name;
		FLand Land;
		FAsk Ask;
		FCourse Course;
		FTimes Times;
	};
	std::vector<FScene> Scenes;

	FCourse Scene(const std::string& Name, const FLand& Land, const FAsk& Ask)
	{
		FScene Made;
		Made.Name = Name;
		Made.Land = Land;
		Made.Ask = Ask;
		Made.Course = PlanOn(Land, Ask);
		Made.Times = Time(Made.Course, Seconds(Made.Course.Length), Helm);
		Scenes.push_back(Made);
		const FCourse& C = Scenes.back().Course;
		// What holds of every course.
		Check(C.Points.size() >= 2 && C.Points.size() == C.Heads.size() && C.Points.size() == C.Radii.size(), Name + ": a course of points, each with its heading and the radius of its turn");
		Check(Size(C.Points.front() - Ask.Way.front()) < 1.0e-9 && Size(C.Points.back() - Ask.Way.back()) < 1.0e-9, Name + ": it begins and ends where the way does");
		Check(Size(C.Heads.front() - Unit(Ask.Bows, FSpot{})) < 1.0e-9, Name + ": it begins on her bows");
		Check(Size(C.Heads.back() - Unit(Ask.Rest, FSpot{})) < 1.0e-9, Name + ": it ends as the table wants her");
		bool bUnit = true, bFinite = true;
		for (size_t K = 0; K < C.Points.size(); ++K)
		{
			bUnit = bUnit && Near(Size(C.Heads[K]), 1.0, 1.0e-9);
			bFinite = bFinite && std::isfinite(C.Points[K].X) && std::isfinite(C.Points[K].Y);
		}
		Check(bUnit && bFinite, Name + ": every heading is a unit and every point a number");
		{
			// (its marks: the way come from point to point, and a little for each turn made where she lies, as a circle
			// too small to see would count)
			const std::vector<double> Lie = MarksOf(C);
			bool bMarks = C.Marks.size() == C.Points.size() && C.Marks.front() == 0.0 && C.Marks.back() == C.Length;
			for (size_t K = 1; bMarks && K < C.Marks.size(); ++K)
			{
				const double Step = C.Marks[K] - C.Marks[K - 1], Lay = Lie[K] - Lie[K - 1];
				bMarks = Step >= Lay - 1.0e-9 && (Lay > 1.0e-9 ? Step <= Lay + 1.0e-9 : Step > 0.0 && Step <= 0.04 * Ask.Step * 0.08);
			}
			Check(bMarks && C.Length >= Lie.back() - 1.0e-9 && C.Length <= Lie.back() + 0.04 * Ask.Step * Turned(C) + 1.0e-6, Name + ": its marks are the way come, with a little for each turn made where she lies, and its length the last of them");
		}
		Check(LeastOff(Land, C, Ask.Step * 0.25, Ask.Step) > -Ask.Step * 0.6, Name + ": no point of it is ashore");
		Check(Jump(C) < 0.09, Name + ": her heading never jumps from one point to the next, on a circle or turning where she lies");
		const std::string Wrong = TimesWrong(C, Scenes.back().Times, Seconds(C.Length));
		Check(Wrong.empty(), Name + ": her times: " + Wrong);
		return C;
	}

	FAsk Ask(std::vector<FSpot> Way, FSpot Bows, FSpot Rest, double Turn, double Sweep, double Berth)
	{
		FAsk Made;
		Made.Way = std::move(Way);
		Made.Bows = Bows;
		Made.Rest = Rest;
		Made.Turn = Turn;
		Made.Sweep = Sweep;
		Made.Berth = Berth;
		Made.Step = 0.75;
		return Made;
	}

	const FSpot North{ 0.0, 1.0 }, East{ 1.0, 0.0 };

	/** The widest circle a corner of a way may be swept on, as the plan reckons it: no wider than lets her be on it and off it within the shorter leg. */
	double WidestSweep(const std::vector<FSpot>& Way, size_t K, double Sweep)
	{
		const FSpot In = Unit(Way[K] - Way[K - 1], East), On = Unit(Way[K + 1] - Way[K], East);
		const double Bend = std::fabs(std::atan2(Cross(In, On), Dot(In, On)));
		const double Short = Least(Size(Way[K] - Way[K - 1]), Size(Way[K + 1] - Way[K]));
		return Clamp(Sweep, 0.0, 0.45 * Short / std::tan(Bend * 0.5));
	}

	void Courses()
	{
		const FLand Open;
		{
			const FCourse C = Scene("open sea, dead ahead", Open, Ask({ { 0, 0 }, { 0, 100 } }, North, North, 13.0, 45.0, 4.0));
			bool bStraight = true;
			for (size_t K = 0; K < C.Points.size(); ++K)
			{
				bStraight = bStraight && Near(C.Points[K].X, 0.0, 1.0e-9) && Size(C.Heads[K] - North) < 1.0e-9 && C.Radii[K] == 0.0;
			}
			Check(bStraight && Near(C.Length, 100.0, 1.0e-9) && C.Arcs == 0 && C.Corners == 0, "dead ahead: one straight, no turn at all");
		}
		{
			const FCourse C = Scene("open sea, abeam", Open, Ask({ { 0, 0 }, { 100, 0 } }, North, North, 13.0, 45.0, 4.0));
			Check(C.Arcs == 2 && C.Corners == 0 && Near(C.Tightest, 13.0, 1.0e-9), "abeam: out on her turning circle and in on it, both whole");
			Check(C.Length > 100.0 && C.Length < 100.0 + 13.0 * 3.2, "abeam: a little longer than the straight line, and no great loop");
			// (the first circle is gone round toward where she is going)
			Check(C.Points[1].X > 0.0, "abeam: she turns toward where she is going");
			// (every point of the first arc is on her circle)
			bool bOn = true;
			for (size_t K = 0; K < C.Points.size() && C.Points[K].Y > -1.0e-9 && C.Points[K].X < 13.0; ++K)
			{
				bOn = bOn && Near(Size(C.Points[K] - FSpot{ 13.0, 0.0 }), 13.0, 1.0e-6) && (K == 0 || C.Radii[K] == 13.0);
			}
			Check(bOn, "abeam: the arc out lies on a circle of her own radius beside her");
		}
		{
			const FCourse C = Scene("open sea, astern", Open, Ask({ { 0, 0 }, { 0, -100 } }, North, North, 13.0, 45.0, 4.0));
			Check(C.Arcs == 2 && C.Corners == 0, "astern: she turns about on her circle at each end");
			Check(C.Length < 100.0 + WholeTurn * 13.0 + 30.0, "astern: two half circles and a straight, no more");
		}
		{
			// A short hop: her circles are made to fit it.
			const FCourse C = Scene("a short hop abeam", Open, Ask({ { 0, 0 }, { 14, 0 } }, North, North, 13.0, 45.0, 4.0));
			Check(C.Tightest <= 0.3 * 14.0 + 1.0e-9, "a short hop: the circle is no more than three tenths of the way");
			Check(C.Length < 14.0 * 2.2, "a short hop: no great loop for a short way");
		}
		{
			// A cape: the way bends round an island, which is inside the turn. She sweeps round it on a wide circle that
			// passes a berth outside the corner.
			FLand Land;
			Land.Isles.push_back({ FSpot{ 0.0, 0.0 }, 30.0 });
			const std::vector<FSpot> Way = { { -60, -10 }, { 0, 45 }, { 60, -10 } };
			const FSpot In = Unit(Way[1] - Way[0], East), Out = Unit(Way[2] - Way[1], East);
			const FCourse C = Scene("round a cape", Land, Ask(Way, In, Out, 0.0, 45.0, 4.0));
			Check(C.Arcs == 1 && Near(C.Tightest, WidestSweep(Way, 1, 45.0), 1.0e-9), "a cape: one sweep, as wide as the legs allow (" + std::to_string(C.Tightest) + ")");
			Check(LeastOff(Land, C, 0.2, 0.0) >= LeastOffWay(Land, Way, 0.2) - 1.0e-6, "a cape: she is never nearer the land than the way was");
			double Furthest = -1.0e9;
			for (size_t K = 0; K < C.Points.size(); ++K)
			{
				Furthest = C.Points[K].Y > Furthest ? C.Points[K].Y : Furthest;
			}
			Check(Near(Furthest, 45.0 + 4.0, 0.1), "a cape: the sweep passes a berth outside the corner, to seaward (" + std::to_string(Furthest - 45.0) + ")");
			// (with no circle of her own at the ends she lines up on the sweep where she lies: a small turn, not a spin)
			Check(C.Corners <= 2 && Turned(C) < 1.8 * std::fabs(std::atan2(Cross(In, Out), Dot(In, Out))), "a cape: no more turning than the cape asks, and a little to line up (" + std::to_string(Turned(C)) + ")");
		}
		{
			// A narrows: land inside the turn and land close outside it. The sweep is made less until it fits.
			FLand Land;
			Land.Isles.push_back({ FSpot{ 16.0, 40.0 }, 12.0 });
			Land.Isles.push_back({ FSpot{ -8.0, 62.0 }, 10.0 });
			const std::vector<FSpot> Way = { { 0, 0 }, { 0, 50 }, { 50, 80 } };
			const FCourse C = Scene("through a narrows", Land, Ask(Way, North, Unit(Way[2] - Way[1], East), 0.0, 45.0, 4.0));
			const double Widest = WidestSweep(Way, 1, 45.0);
			Check(C.Arcs == 1 && C.Tightest < Widest - 1.0e-9 && C.Tightest >= Widest / 8.0 - 1.0e-9, "a narrows: the sweep made less to fit, and kept (" + std::to_string(C.Tightest) + " of " + std::to_string(Widest) + ")");
		}
		{
			// No room at all (land hard by the corner outside, and hard by the leg): she turns where she lies, and the
			// course is the way as the map found it.
			FLand Land;
			Land.Isles.push_back({ FSpot{ 16.0, 40.0 }, 12.0 });
			Land.Isles.push_back({ FSpot{ -5.0, 58.0 }, 9.2 });
			Land.Isles.push_back({ FSpot{ -4.1, 30.0 }, 4.0 });
			const std::vector<FSpot> Way = { { 0, 0 }, { 0, 50 }, { 50, 80 } };
			const FCourse C = Scene("no room to round it", Land, Ask(Way, North, Unit(Way[2] - Way[1], East), 0.0, 45.0, 4.0));
			Check(C.Arcs == 0 && C.Corners == 1, "no room: the corner is left a corner, where she turns as she lies");
			Check(Near(C.Length, Size(Way[1] - Way[0]) + Size(Way[2] - Way[1]), 0.1), "no room: the course is the way, to a tenth of a unit");
			double Off = 0.0;
			for (size_t K = 0; K < C.Points.size(); ++K)
			{
				// (how far each point lies off the way's two legs)
				const FSpot P = C.Points[K];
				const FSpot OnSecond = Unit(Way[2] - Way[1], East);
				const double First = P.Y <= 50.0 ? std::fabs(P.X) : 1.0e9, Second = std::fabs(Cross(OnSecond, P - Way[1]));
				Off = Least(First, Second) > Off ? Least(First, Second) : Off;
			}
			Check(Off < 0.08, "no room: no point of it is off the way (" + std::to_string(Off) + ")");
		}
		{
			// Lying along a shore, going to a place inshore and astern: she turns out to sea, not across the beach.
			FLand Land;
			Land.bShore = true;
			const FCourse C = Scene("off a lee shore", Land, Ask({ { 0, 2 }, { -80, 60 } }, East, East, 13.0, 45.0, 4.0));
			Check(C.Arcs == 2 && C.Corners == 0 && Near(C.Tightest, 13.0, 1.0e-9), "a shore: both circles whole");
			Check(C.Points[1].Y > 2.0, "a shore: she turns away from the land");
		}
		{
			// Hemmed in where she lies (a harbour): no circle fits, and she turns where she lies, as she did before.
			FLand Land;
			Land.Isles.push_back({ FSpot{ 5.0, 0.0 }, 4.0 });
			Land.Isles.push_back({ FSpot{ -5.0, 0.0 }, 4.0 });
			Land.Isles.push_back({ FSpot{ 0.0, -5.0 }, 4.0 });
			const FCourse C = Scene("out of a harbour", Land, Ask({ { 0, 0 }, { 0, 90 }, { 60, 120 } }, East, East, 13.0, 45.0, 4.0));
			// (she does not leave where she lies until she heads the way she is going, and then goes straight out; and
			// she has swung the short way to it)
			size_t Swung = 0;
			double Swing = 0.0;
			while (Swung + 1 < C.Points.size() && Size(C.Points[Swung + 1] - C.Points[0]) < 1.0e-9)
			{
				++Swung;
				Swing += std::fabs(std::atan2(Cross(C.Heads[Swung - 1], C.Heads[Swung]), Dot(C.Heads[Swung - 1], C.Heads[Swung])));
			}
			const FSpot Leg = Unit(C.Points[Swung + 1] - C.Points[0], East);
			Check(Swung >= 2 && Size(C.Heads[Swung] - Leg) < 1.0e-6 && Size(C.Heads[Swung + 1] - Leg) < 1.0e-6 && Leg.Y > 0.9
				&& Near(Swing, std::fabs(std::atan2(Cross(East, Leg), Dot(East, Leg))), 1.0e-6), "a harbour: she turns where she lies, the short way, and goes straight out");
			Check(C.Corners == 1 && C.Arcs == 2, "a harbour: the corner out at sea is swept, and she comes in on her circle");
		}
		{
			// A long passage with three corners in open water: all swept wide, her own circles at the ends.
			const std::vector<FSpot> Way = { { 0, 0 }, { 120, 40 }, { 160, 160 }, { 300, 150 }, { 420, 260 } };
			const FCourse C = Scene("a long passage", Open, Ask(Way, North, North, 13.0, 45.0, 4.0));
			Check(C.Arcs == 5 && C.Corners == 0 && Near(C.Tightest, 13.0, 1.0e-9), "a passage: five circles, the sweeps at forty-five and her own at thirteen");
			int Wide = 0;
			for (size_t K = 0; K < C.Radii.size(); ++K)
			{
				Wide += C.Radii[K] == 45.0 ? 1 : 0;
			}
			Check(Wide > 30, "a passage: the corners are on the wide circle (" + std::to_string(Wide) + " points of it)");
		}
		{
			// The same problem seen in a mirror is the same course in a mirror.
			const std::vector<FSpot> Way = { { 0, 0 }, { 120, 40 }, { 160, 160 } };
			std::vector<FSpot> Mirrored = Way;
			for (FSpot& P : Mirrored)
			{
				P.X = -P.X;
			}
			const FCourse A = PlanOn(Open, Ask(Way, East, North, 13.0, 45.0, 4.0));
			const FCourse B = PlanOn(Open, Ask(Mirrored, FSpot{ -1.0, 0.0 }, North, 13.0, 45.0, 4.0));
			bool bSame = A.Points.size() == B.Points.size() && Near(A.Length, B.Length, 1.0e-9);
			for (size_t K = 0; bSame && K < A.Points.size(); ++K)
			{
				bSame = Near(A.Points[K].X, -B.Points[K].X, 1.0e-9) && Near(A.Points[K].Y, B.Points[K].Y, 1.0e-9);
			}
			Check(bSame, "a mirror: left and right are treated alike");
		}
		{
			// Nothing asked: the way comes back as it went in, with her turns made where she lies.
			const std::vector<FSpot> Way = { { 0, 0 }, { 120, 40 }, { 160, 160 } };
			const FCourse C = Scene("no circles asked", Open, Ask(Way, North, North, 0.0, 0.0, 0.0));
			Check(C.Arcs == 0 && C.Corners == 3 && Near(C.Length, Size(Way[1] - Way[0]) + Size(Way[2] - Way[1]), 0.2), "no circles asked: the way, and three turns where she lies");
			FAsk One = Ask({ { 3, 4 } }, North, East, 13.0, 45.0, 4.0);
			const FCourse Lone = Plan(One, [](FSpot) { return true; });
			Check(Lone.Points.size() == 1 && Time(Lone, 3.0, Helm).At.size() == 2, "a way of one point: one point, and no fault");
		}
	}

	/** Many seas made by chance (the same every run): islands scattered, a way of two to four legs found clear of them, and the course asked. */
	void ManySeas()
	{
		unsigned long long Seed = 0x2545F4914F6CDD1DULL;
		auto Chance = [&Seed]()
		{
			Seed = Seed * 6364136223846793005ULL + 1442695040888963407ULL;
			return static_cast<double>((Seed >> 33) & 0xFFFFFFULL) / static_cast<double>(0x1000000ULL);
		};
		int Made = 0, Ashore = 0, Wrong = 0, Jumped = 0, Whole = 0, Rounded = 0, Left = 0, Late = 0, Hurried = 0;
		double Slowest = 0.0, Swung = 0.0, LongestTime = 0.0, Quickest = 0.0;
		std::string FirstLate;
		for (int Trial = 0; Trial < 4000 && Made < 600; ++Trial)
		{
			FLand Land;
			const int Isles = 2 + static_cast<int>(Chance() * 9.0);
			for (int K = 0; K < Isles; ++K)
			{
				Land.Isles.push_back({ FSpot{ Chance() * 300.0, Chance() * 300.0 }, 6.0 + Chance() * 30.0 });
			}
			std::vector<FSpot> Way;
			const int Legs = 1 + static_cast<int>(Chance() * 3.0);
			bool bClear = true;
			for (int K = 0; K <= Legs && bClear; ++K)
			{
				const FSpot P{ Chance() * 300.0, Chance() * 300.0 };
				bClear = Land.Off(P) > 1.0 && (Way.empty() || Size(P - Way.back()) > 12.0);
				if (bClear && !Way.empty())
				{
					const int Asks = static_cast<int>(Size(P - Way.back()) / 0.3) + 1;
					for (int At = 0; At <= Asks && bClear; ++At)
					{
						bClear = Land.Off(Way.back() + (P - Way.back()) * (static_cast<double>(At) / Asks)) > 0.5;
					}
				}
				Way.push_back(P);
			}
			if (!bClear)
			{
				continue;
			}
			++Made;
			const double BowsAt = Chance() * WholeTurn, RestAt = Chance() < 0.7 ? BowsAt : Chance() * WholeTurn;
			const FAsk Asked = Ask(Way, FSpot{ std::cos(BowsAt), std::sin(BowsAt) }, FSpot{ std::cos(RestAt), std::sin(RestAt) }, 4.0 + Chance() * 14.0, 10.0 + Chance() * 50.0, 1.0 + Chance() * 5.0);
			const FCourse C = PlanOn(Land, Asked);
			Wrong += (C.Points.size() < 2 || Size(C.Points.front() - Way.front()) > 1.0e-9 || Size(C.Points.back() - Way.back()) > 1.0e-9
				|| Size(C.Heads.front() - Asked.Bows) > 1.0e-9 || Size(C.Heads.back() - Asked.Rest) > 1.0e-9 || !std::isfinite(C.Length) || C.Length > 12.0 * (MarksOfWay(Way) + 40.0)) ? 1 : 0;
			Ashore += LeastOff(Land, C, 0.2, Asked.Step) < -0.45 ? 1 : 0;
			Jumped += Jump(C) >= 0.09 ? 1 : 0;
			Whole += C.Corners == 0 ? 1 : 0;
			Rounded += C.Arcs;
			Left += C.Corners;
			const double Wanted = Seconds(C.Length);
			const FTimes T = Time(C, Wanted, Helm);
			const std::string Said = TimesWrong(C, T, Wanted);
			if (!Said.empty())
			{
				++Late;
				FirstLate = FirstLate.empty() ? Said : FirstLate;
			}
			Slowest = T.Seconds / Wanted > Slowest ? T.Seconds / Wanted : Slowest;
			LongestTime = T.Seconds > LongestTime ? T.Seconds : LongestTime;
			Hurried += T.Helm > Helm * 1.001 ? 1 : 0;
			Quickest = T.Helm > Quickest ? T.Helm : Quickest;
			Swung = T.Swing > Swung ? T.Swing : Swung;
		}
		Check(Made >= 400, "many seas: enough ways found to try (" + std::to_string(Made) + ")");
		Check(Wrong == 0, "many seas: every course begins and ends as asked, and is no endless loop (" + std::to_string(Wrong) + " did not)");
		Check(Ashore == 0, "many seas: no course goes ashore (" + std::to_string(Ashore) + " did)");
		Check(Jumped == 0, "many seas: her heading never jumps from one point to the next (" + std::to_string(Jumped) + " did)");
		Check(Late == 0, "many seas: her times are right on every course (" + std::to_string(Late) + " were not; the first: " + FirstLate + ")");
		Check(Whole * 10 >= Made * 6, "many seas: most courses have a circle at every turn (" + std::to_string(Whole) + " of " + std::to_string(Made) + ")");
		// (these seas are harder than the map's: three legs at any angle, and she must come round to how she lay)
		Check(Hurried * 2 <= Made && Quickest < 8.0 * Helm * 0.999, "many seas: her helm is made quicker on under half the courses, and never to its limit (" + std::to_string(Hurried) + " of " + std::to_string(Made) + ", the quickest " + std::to_string(Quickest) + ")");
		std::printf("many seas: %d ways, %d with a circle at every turn, %d circles kept, %d turns made where she lies; the slowest took %.2f of the time asked and the longest %.2f s; %d were given a quicker helm (the quickest %.2f radians a second); her head never swung faster than %.2f\n", Made, Whole, Rounded, Left, Slowest, LongestTime, Hurried, Quickest, Swung);
	}

	/** Where she is a share of the way through her time: how far she has run, as a share of the course. */
	double RanAt(const FTimes& T, double Share)
	{
		const double At = Clamp(Share, 0.0, 1.0) * static_cast<double>(T.Ran.size() - 1);
		const size_t K = static_cast<size_t>(At) + 1 < T.Ran.size() ? static_cast<size_t>(At) : T.Ran.size() - 2;
		const double F = At - static_cast<double>(K);
		return (T.Ran[K] + (T.Ran[K + 1] - T.Ran[K]) * F) / T.Ran.back();
	}

	void HerWay()
	{
		const FLand Open;
		// A long straight run: nothing but her own weight.
		{
			const FCourse C = PlanOn(Open, Ask({ { 0, 0 }, { 0, 600 } }, North, North, 13.0, 45.0, 4.0));
			const double Asked = Seconds(C.Length);
			const FTimes T = Time(C, Asked, Helm);
			Check(TimesWrong(C, T, Asked).empty(), "her way, a straight run: " + TimesWrong(C, T, Asked));
			Check(T.Seconds >= Asked + 0.2 && T.Seconds <= Asked + 0.26, "her way: on a straight run she takes the time asked, and the fifth of a second her head is eased over");
			Check(T.Fastest < 1.6 * C.Length / Asked && Near(T.Fastest, T.Full, T.Full * 0.01), "her way: her full speed is no more than about half as fast again as her mean, and she reaches it");
			Check(RanAt(T, 1.0 / 3.0) < 0.3, "her way: she is slow to gather it (under three tenths of the course in the first third of her time)");
			Check(1.0 - RanAt(T, 0.7) < 0.2, "her way: she carries it in (under a fifth of the course left with three tenths of her time to go)");
			// (no change of speed is sudden: from one tick to the next, never more than she can gather in that time)
			double Sharpest = 0.0;
			for (size_t K = 2; K < T.Ran.size(); ++K)
			{
				const double Change = std::fabs((T.Ran[K] - T.Ran[K - 1]) - (T.Ran[K - 1] - T.Ran[K - 2])) / (T.Tick * T.Tick);
				Sharpest = Change > Sharpest ? Change : Sharpest;
			}
			Check(Sharpest < 1.05 * T.Full / (Gather * Asked), "her way: no change of speed is sudden (" + std::to_string(Sharpest) + ")");
			Check(T.Swing == 0.0, "her way: on a straight run her head does not move");
		}
		// Out on her own circle: she creeps round it, as fast as her helm lets her head swing and no faster.
		{
			const FCourse C = PlanOn(Open, Ask({ { 0, 0 }, { 300, 0 } }, North, North, 13.0, 45.0, 4.0));
			const double Asked = Seconds(C.Length);
			const FTimes T = Time(C, Asked, Helm);
			Check(TimesWrong(C, T, Asked).empty(), "her way, out on her circle: " + TimesWrong(C, T, Asked));
			Check(T.Helm == Helm, "her way: an easy passage is sailed on the helm asked (" + std::to_string(T.Helm) + ")");
			Check(T.Swing > Helm * 0.9 && T.Swing <= Helm * 1.02, "her way: round her own circle her head swings as fast as her helm allows, and no faster (" + std::to_string(T.Swing) + ")");
			double OnCircle = 0.0;
			for (size_t K = 1; K < T.At.size(); ++K)
			{
				// (while she is still on the circle she left on)
				if (T.At[K].X < 12.0 && T.At[K].Y > -1.0e-9 && T.At[K].X > 0.0)
				{
					const double Went = (T.Ran[K] - T.Ran[K - 1]) / T.Tick;
					OnCircle = Went > OnCircle ? Went : OnCircle;
				}
			}
			Check(OnCircle > 0.0 && OnCircle <= Helm * 13.0 * 1.01, "her way: on the circle she goes no faster than her helm times its radius (" + std::to_string(OnCircle) + ")");
			Check(T.Fastest > 2.0 * Helm * 13.0, "her way: and on the straight after it she works up to far more (" + std::to_string(T.Fastest) + ")");
		}
		// A turn made where she lies: she all but stops for it, and it takes as long as the swing takes.
		{
			const std::vector<FSpot> Way = { { 0, 0 }, { 0, 200 }, { 200, 200 } };
			const FCourse C = PlanOn(Open, Ask(Way, North, East, 0.0, 0.0, 0.0));
			const double Asked = Seconds(C.Length);
			const FTimes T = Time(C, Asked, Helm);
			Check(C.Corners == 1 && TimesWrong(C, T, Asked).empty(), "her way, a turn where she lies: " + TimesWrong(C, T, Asked));
			int Turning = 0;
			double Crept = 0.0;
			for (size_t K = 1; K < T.Head.size(); ++K)
			{
				if (T.Head[K].X > 0.05 && T.Head[K].X < 0.999)
				{
					++Turning;
					Crept += T.Ran[K] - T.Ran[K - 1];
				}
			}
			const double Took = Turning * T.Tick, Swing = WholeTurn / 4.0 / Helm;
			Check(Took > Swing * 0.9 && Took < Swing + 0.45, "her way: a quarter turn where she lies takes as long as her helm takes to swing it (" + std::to_string(Took) + " s)");
			Check(Crept < 1.0, "her way: and she all but stops to make it (" + std::to_string(Crept) + " units run while turning)");
		}
		bool bLonger = true, bRising = true;
		for (double Long = 5.0; Long < 2500.0; Long += 5.0)
		{
			const double Was = Clamp(1.1 + Long / 380.0, 1.4, 3.6); // (ships that sail, 24 November: 1.1 + world units / 3800)
			bLonger = bLonger && Seconds(Long) >= 1.25 * Was;
			bRising = bRising && Seconds(Long + 5.0) >= Seconds(Long);
		}
		Check(bLonger && bRising, "her time: at least a quarter longer than she took before, and longer for a longer way");
		Check(Seconds(0.0) >= 2.8 && Seconds(1.0e6) <= 6.5, "her time: from under three seconds to six and a half");
		Check(Straighten(0.86) == 0.0 && Straighten(1.0) == 1.0 && Straighten(0.5) == 0.0, "her wake: straightened astern only as she comes to rest");
	}

	/** Where a point of the wake lies on the table, from where it is in her frame. */
	FSpot OnTable(FSpot Now, FSpot Head, double Scale, double X, double Y)
	{
		return Now - Left(Head) * (X * Scale) + Head * (Y * Scale);
	}

	void HerWake()
	{
		// A straight course: the wake is where it was built, straight astern.
		{
			const std::vector<FSpot> Points = { { 0, 0 }, { 0, 200 } }, Heads = { North, North };
			const std::vector<double> Marks = { 0.0, 200.0 };
			FAstern Astern;
			LookAstern(Points.data(), Heads.data(), Marks.data(), 2, 120.0, 90.0, 48, Astern);
			bool bSame = true;
			for (const FSpot& At : { FSpot{ 0.0, -10.0 }, FSpot{ 3.5, -44.0 }, FSpot{ -6.0, -89.9 }, FSpot{ 2.0, 5.0 } })
			{
				double X = 0.0, Y = 0.0;
				Trail(Astern, North, 1.0, 0.0, 0.0, At.X, At.Y, X, Y);
				bSame = bSame && Near(X, At.X, 1.0e-9) && Near(Y, At.Y, 1.0e-9);
				Trail(Astern, North, 2.0, 0.0, 0.0, At.X * 0.5, At.Y * 0.5, X, Y);
				bSame = bSame && Near(X, At.X * 0.5, 1.0e-9) && Near(Y, At.Y * 0.5, 1.0e-9);
			}
			Check(bSame, "her wake: on a straight course it lies straight astern, at any size of piece");
			// (before she has run its length, the rest of it lies astern of where she lay)
			LookAstern(Points.data(), Heads.data(), Marks.data(), 2, 5.0, 90.0, 48, Astern);
			double X = 0.0, Y = 0.0;
			Trail(Astern, North, 1.0, 0.0, 0.0, 0.0, -30.0, X, Y);
			Check(Near(X, 0.0, 1.0e-9) && Near(Y, -30.0, 1.0e-9), "her wake: just under way, it still lies astern of where she lay");
		}
		// On a circle: the wake lies round the circle, not along her keel.
		{
			std::vector<FSpot> Points, Heads;
			std::vector<double> Marks;
			const double Radius = 20.0;
			for (int Degree = 0; Degree <= 270; ++Degree)
			{
				const double A = Degree * WholeTurn / 360.0;
				Points.push_back(FSpot{ Radius * std::cos(A), Radius * std::sin(A) });
				Heads.push_back(FSpot{ -std::sin(A), std::cos(A) });
				Marks.push_back(Radius * A);
			}
			const int Count = static_cast<int>(Points.size());
			const double Ran = Radius * WholeTurn * 0.5; // (half way round)
			FSpot Now, Head;
			Along(Points.data(), Heads.data(), Marks.data(), Count, Ran, Now, Head);
			Check(Near(Now.X, -Radius, 0.01) && Near(Now.Y, 0.0, 0.01) && Near(Head.Y, -1.0, 1.0e-3), "her wake: half way round a circle she is where she should be");
			FAstern Astern;
			LookAstern(Points.data(), Heads.data(), Marks.data(), Count, Ran, 45.0, 64, Astern);
			bool bRound = true, bAbeam = true;
			for (double Back = 2.0; Back < 44.0; Back += 3.0)
			{
				double X = 0.0, Y = 0.0;
				Trail(Astern, Head, 1.0, 0.0, 0.0, 0.0, -Back, X, Y);
				bRound = bRound && Near(Size(OnTable(Now, Head, 1.0, X, Y)), Radius, 0.05);
				Trail(Astern, Head, 1.0, 0.0, 0.0, 3.0, -Back, X, Y);
				bAbeam = bAbeam && Near(std::fabs(Size(OnTable(Now, Head, 1.0, X, Y)) - Radius), 3.0, 0.05);
			}
			Check(bRound, "her wake: on a circle it lies round the circle she has sailed");
			Check(bAbeam, "her wake: and a point abeam of her keel stays as far abeam of her course");
			Check(!Near(Size(OnTable(Now, Head, 1.0, 0.0, -30.0)), Radius, 5.0), "her wake: (straight astern it would lie well off the circle)");
			double X = 0.0, Y = 0.0;
			Trail(Astern, Head, 1.0, 1.0, 0.0, 1.5, -30.0, X, Y);
			Check(Near(X, 1.5, 1.0e-12) && Near(Y, -30.0, 1.0e-12), "her wake: come to rest, it lies straight astern again");
			double HalfX = 0.0, HalfY = 0.0, BentX = 0.0, BentY = 0.0;
			Trail(Astern, Head, 1.0, 0.0, 0.0, 1.5, -30.0, BentX, BentY);
			Trail(Astern, Head, 1.0, 0.5, 0.0, 1.5, -30.0, HalfX, HalfY);
			Check(Near(HalfX, (BentX + 1.5) * 0.5, 1.0e-12) && Near(HalfY, (BentY - 30.0) * 0.5, 1.0e-12), "her wake: and half way to rest, half way back");
			Trail(Astern, Head, 1.0, 0.0, 0.0, 2.0, 6.0, X, Y);
			Check(Near(X, 2.0, 1.0e-12) && Near(Y, 6.0, 1.0e-12), "her wake: what is forward of her middle is hers and stays");
			// A piece is a formation that turns as one: her sterns, eight units astern of her middle, swing wide of the
			// circle her middle sails, and the wake lies where they swung (a wider circle), joined to them.
			const double Stiff = 8.0, Wide = std::sqrt(Radius * Radius + Stiff * Stiff);
			bool bStern = true, bHers = true;
			for (double Back = 0.5; Back < 30.0; Back += 2.5)
			{
				Trail(Astern, Head, 1.0, 0.0, Stiff, 0.0, -Stiff - Back, X, Y);
				bStern = bStern && Near(Size(OnTable(Now, Head, 1.0, X, Y)), Wide, 0.05);
				Trail(Astern, Head, 1.0, 0.0, Stiff, 1.5, -Stiff + Back * 0.25, X, Y);
				bHers = bHers && Near(X, 1.5, 1.0e-12) && Near(Y, -Stiff + Back * 0.25, 1.0e-12);
			}
			Check(bStern, "her wake: it lies round the wider circle her sterns have swung");
			Check(bHers, "her wake: and from her sterns forward she is herself, and stays");
			Trail(Astern, Head, 1.0, 0.0, Stiff, 1.5, -Stiff - 1.0e-9, X, Y);
			Check(Near(X, 1.5, 1.0e-6) && Near(Y, -Stiff, 1.0e-6), "her wake: it joins her sterns with no step");
			// (and at half the piece's size: the same water, in her own smaller units)
			Trail(Astern, Head, 2.0, 0.0, Stiff * 0.5, 0.0, (-Stiff - 12.0) * 0.5, X, Y);
			Check(Near(Size(OnTable(Now, Head, 2.0, X, Y)), Wide, 0.05), "her wake: the same at any size of piece");
		}
		// A turn made where she lay (no room for her circle): the wake before the start lies astern of how she lay.
		{
			const std::vector<FSpot> Points = { { 0, 0 }, { 0, 0 }, { 0, 100 } }, Heads = { East, North, North };
			const std::vector<double> Marks = { 0.0, 0.0, 100.0 };
			FAstern Astern;
			LookAstern(Points.data(), Heads.data(), Marks.data(), 3, 5.0, 60.0, 61, Astern);
			double X = 0.0, Y = 0.0;
			Trail(Astern, North, 1.0, 0.0, 0.0, 0.0, -30.0, X, Y);
			const FSpot Lies = OnTable(FSpot{ 0.0, 5.0 }, North, 1.0, X, Y);
			Check(Near(Lies.X, -25.0, 1.0e-6) && Near(Lies.Y, 0.0, 1.0e-6), "her wake: turned where she lay, the old wake stays astern of how she lay");
		}
	}

	void Write(const char* Path)
	{
		std::ofstream File(Path);
		File << "{\"helm\":" << Helm << ",\"scenes\":[";
		for (size_t S = 0; S < Scenes.size(); ++S)
		{
			const FScene& Scene_ = Scenes[S];
			const FTimes& T = Scene_.Times;
			File << (S ? ",\n" : "\n") << "{\"name\":\"" << Scene_.Name << "\",\"shore\":" << (Scene_.Land.bShore ? "true" : "false") << ",\"isles\":[";
			for (size_t K = 0; K < Scene_.Land.Isles.size(); ++K)
			{
				File << (K ? "," : "") << "[" << Scene_.Land.Isles[K].Mid.X << "," << Scene_.Land.Isles[K].Mid.Y << "," << Scene_.Land.Isles[K].Radius << "]";
			}
			File << "],\"way\":[";
			for (size_t K = 0; K < Scene_.Ask.Way.size(); ++K)
			{
				File << (K ? "," : "") << "[" << Scene_.Ask.Way[K].X << "," << Scene_.Ask.Way[K].Y << "]";
			}
			File << "],\"bows\":[" << Scene_.Ask.Bows.X << "," << Scene_.Ask.Bows.Y << "],\"rest\":[" << Scene_.Ask.Rest.X << "," << Scene_.Ask.Rest.Y << "],\"course\":[";
			for (size_t K = 0; K < Scene_.Course.Points.size(); ++K)
			{
				File << (K ? "," : "") << "[" << Scene_.Course.Points[K].X << "," << Scene_.Course.Points[K].Y << "," << Scene_.Course.Heads[K].X << "," << Scene_.Course.Heads[K].Y << "]";
			}
			// (her times: where she is at each tick, her heading, and how far she has run)
			File << "],\"length\":" << Scene_.Course.Length << ",\"seconds\":" << T.Seconds << ",\"asked\":" << Seconds(Scene_.Course.Length) << ",\"tick\":" << T.Tick
				<< ",\"full\":" << T.Full << ",\"fastest\":" << T.Fastest << ",\"swing\":" << T.Swing << ",\"gave\":" << T.Helm << ",\"arcs\":" << Scene_.Course.Arcs << ",\"corners\":" << Scene_.Course.Corners << ",\"at\":[";
			for (size_t K = 0; K < T.At.size(); ++K)
			{
				File << (K ? "," : "") << "[" << T.At[K].X << "," << T.At[K].Y << "," << T.Head[K].X << "," << T.Head[K].Y << "," << T.Ran[K] << "]";
			}
			// (her wake at three fifths of her time: its middle line, forty stations astern over ninety units)
			File << "],\"wake\":[";
			{
				const std::vector<double>& Marks = Scene_.Course.Marks;
				FAstern Astern;
				LookAstern(Scene_.Course.Points.data(), Scene_.Course.Heads.data(), Marks.data(), static_cast<int>(Marks.size()), T.Ran[T.Ran.size() * 3 / 5], 90.0, 40, Astern);
				for (size_t K = 0; K < Astern.At.size(); ++K)
				{
					File << (K ? "," : "") << "[" << Astern.At[K].X << "," << Astern.At[K].Y << "]";
				}
			}
			File << "]}";
		}
		File << "\n]}\n";
	}
}

int main(int Count, char** Words)
{
	Courses();
	ManySeas();
	HerWay();
	HerWake();
	if (Count > 1)
	{
		Write(Words[1]);
	}
	std::printf("%d checks, %d faults\n", Checks, Faults);
	return Faults == 0 ? 0 : 1;
}
