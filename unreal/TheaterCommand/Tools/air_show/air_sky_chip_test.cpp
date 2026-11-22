// The clouds turned on the graphics chip (the speed gate's GS6): the cloud test of AirSky::Chip and AirSkyChip.inl.
//
// The chip's arithmetic is run here in C++ (air_sky_chip.h), the same text M_CloudTurned runs, and checked against the
// painter's own points (AirSky::Painter::Paint with the sheet's shapes, the whole sky in one painter): every point of
// every puff drawn, its place, its colour, its alpha and its place on the sheet, from eyes in the ground battle's sky
// and in the three recorded air battles'. Then the table laid at one eye and read at another within its slack (the
// chip's own frames between two lays) against a table laid there: the same puffs drawn, the same points. Then a ground
// battle's eye moving through 30 s of shots: how often the rooms turn and what they send, against how often the chip's
// table is laid again. Usage: air_sky_chip_test [recording.json ...]

#include "Battle/AirFeed.h"
#include "Battle/AirGround.h"
#include "Battle/AirShow.h"
#include "Battle/AirSky.h"
#include "air_sky_chip.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace
{
	using AirSky::Vec;
	using Clock = std::chrono::steady_clock;

	struct Limit
	{
		std::string What;
		bool bOk = false;
		std::string Got, Want;
	};

	std::string Num(double V, int Places = 2)
	{
		char Text[64];
		std::snprintf(Text, sizeof(Text), "%.*f", Places, V);
		return Text;
	}

	double Ms(Clock::time_point From) { return std::chrono::duration<double, std::milli>(Clock::now() - From).count(); }

	/** One point as the chip works it out: read from the table by the point's own texture coordinates (Chip::PointPlaces),
	 * as M_CloudTurned's glue reads it. */
	struct ChipReader
	{
		const AirSky::Chip& Ch;
		std::vector<float> Ring, Slot;
		explicit ChipReader(const AirSky::Chip& In) : Ch(In) { Ch.PointPlaces(Ring, Slot); }
		AirSkyChipCpp::FSkyChipOut At(int SlotIndex, int Point, const Vec& Seen) const
		{
			using AirSkyChipCpp::float3;
			using AirSkyChipCpp::float4;
			const std::vector<float>& T = Ch.Table();
			const size_t P = static_cast<size_t>(SlotIndex) * Ch.PerPuff() + static_cast<size_t>(Point);
			const int Col = static_cast<int>(std::floor(Slot[P * 2] + 0.5f)), Row = static_cast<int>(std::floor(Slot[P * 2 + 1] + 0.5f));
			const int K = static_cast<int>(std::floor(Ring[P * 2] + 0.5f)), RingIndex = static_cast<int>(std::floor(Ring[P * 2 + 1] + 0.5f));
			auto Texel = [&T](int R, int C)
			{
				const float* A = &T[(static_cast<size_t>(R) * AirSky::Chip::Wide + static_cast<size_t>(C)) * 4];
				return float4(A[0], A[1], A[2], A[3]);
			};
			AirSkyChipCpp::FSkyChip Chip;
			Chip.Pad = 0.0f;
			return Chip.Vertex(Texel(0, 0), Texel(0, 1), Texel(0, 2), Texel(0, 3), Texel(0, 4), Texel(Row, Col), Texel(Row, Col + 1), Texel(Row, Col + 2), Texel(Row, Col + 3),
				Texel(Row, Col + 4), Texel(Row, Col + 5), Texel(Row, Col + 6), Texel(Row, Col + 7),
				float3(static_cast<float>(Seen.X), static_cast<float>(Seen.Y), static_cast<float>(Seen.Z)), Col / AirSky::Chip::TexelsPerPuff + (Row - 1) * AirSky::Chip::PuffsPerRow, K,
				RingIndex);
		}
		/** Whether the chip draws a slot's puff at all (a puff left out is every point at its middle, no place on the sheet). */
		bool Drawn(int SlotIndex, const Vec& Seen) const
		{
			const AirSkyChipCpp::FSkyChipOut O = At(SlotIndex, 0, Seen);
			return O.Sheet.x != 0.0f || O.Sheet.y != 0.0f;
		}
	};

	int Byte(float V) { return static_cast<int>(std::max(0.0f, std::min(1.0f, V)) * 255.0f + 0.5f); }

	/** The chip's points against the painter's, puff by puff in the order both lay them. */
	struct Match
	{
		long Points = 0, PlaceOff = 0, ColourOff = 0, AlphaOff = 0, SheetOff = 0;
		double MostPlace = 0.0, MostSheet = 0.0;
		int MostColour = 0, MostAlpha = 0, Puffs = 0, PainterOnly = 0, ChipOnly = 0;
		void Add(const Match& O)
		{
			Points += O.Points;
			PlaceOff += O.PlaceOff;
			ColourOff += O.ColourOff;
			AlphaOff += O.AlphaOff;
			SheetOff += O.SheetOff;
			MostPlace = std::max(MostPlace, O.MostPlace);
			MostSheet = std::max(MostSheet, O.MostSheet);
			MostColour = std::max(MostColour, O.MostColour);
			MostAlpha = std::max(MostAlpha, O.MostAlpha);
			Puffs += O.Puffs;
			PainterOnly += O.PainterOnly;
			ChipOnly += O.ChipOnly;
		}
	};

	constexpr double PlaceGive = 0.25; // metres
	constexpr int ColourGive = 2, AlphaGive = 2; // of 255
	constexpr double SheetGive = 0.002;

	Match AgainstPainter(const AirSky::Field& F, const Vec& Eye, const Vec& Drift, const Vec& Sun, const AirSky::Rgb& Light, double Ink, AirSky::Chip& Ch)
	{
		Match M;
		AirSky::Painter Paint;
		Paint.SetSprites(true);
		Paint.SetInk(Ink);
		Paint.SetRoom(Ch.Room());
		std::vector<Vec> Points, Shadows;
		std::vector<uint8_t> Rgba, ShadowRgba;
		std::vector<float> Uvs;
		Paint.Paint(F, Eye, Drift, Sun, Light, Points, Rgba, &Shadows, &ShadowRgba, &Uvs);
		Ch.SetInk(Ink);
		Ch.Lay(F, Eye, Drift, Sun, Light);
		const ChipReader Read(Ch);
		const Vec Seen = Eye - Drift;
		const int Per = Paint.PerPuff();
		const int Spare = Paint.RoomFor(F) - Paint.Drawn();
		std::vector<int> ChipSlots;
		for (int S = 0; S < Ch.Laid(); ++S)
		{
			if (Read.Drawn(S, Seen))
			{
				ChipSlots.push_back(S);
			}
		}
		auto PainterMiddle = [&](int I) { return Points[static_cast<size_t>(Spare + I) * Per]; };
		auto ChipMiddle = [&](int J)
		{
			const AirSkyChipCpp::FSkyChipOut O = Read.At(ChipSlots[static_cast<size_t>(J)], 0, Seen);
			return Vec(O.At.x, O.At.y, O.At.z);
		};
		int I = 0, J = 0;
		const int PainterN = Paint.Drawn(), ChipN = static_cast<int>(ChipSlots.size());
		while (I < PainterN && J < ChipN)
		{
			if ((PainterMiddle(I) - ChipMiddle(J)).Length() > 1.0)
			{
				// (a puff at the edge of being drawn, its fade times its alpha a hair either side of 0.002, drawn by one and
				// not the other: looked for a few puffs on, either way)
				int Ahead = 0;
				for (int Look = 1; Look <= 4 && !Ahead; ++Look)
				{
					if (J + Look < ChipN && (PainterMiddle(I) - ChipMiddle(J + Look)).Length() <= 1.0) Ahead = Look;
					else if (I + Look < PainterN && (PainterMiddle(I + Look) - ChipMiddle(J)).Length() <= 1.0) Ahead = -Look;
				}
				if (Ahead > 0)
				{
					M.ChipOnly += Ahead;
					J += Ahead;
				}
				else if (Ahead < 0)
				{
					M.PainterOnly -= Ahead;
					I -= Ahead;
				}
				else
				{
					++M.PainterOnly;
					++M.ChipOnly;
					++I;
					++J;
				}
				continue;
			}
			++M.Puffs;
			for (int P = 0; P < Per; ++P)
			{
				const size_t Q = static_cast<size_t>(Spare + I) * Per + static_cast<size_t>(P);
				const AirSkyChipCpp::FSkyChipOut O = Read.At(ChipSlots[static_cast<size_t>(J)], P, Seen);
				++M.Points;
				const double Place = (Points[Q] - Vec(O.At.x, O.At.y, O.At.z)).Length();
				M.MostPlace = std::max(M.MostPlace, Place);
				if (Place > PlaceGive) ++M.PlaceOff;
				const int Colour = std::max({ std::abs(Rgba[Q * 4] - Byte(O.Colour.x)), std::abs(Rgba[Q * 4 + 1] - Byte(O.Colour.y)), std::abs(Rgba[Q * 4 + 2] - Byte(O.Colour.z)) });
				const int Alpha = std::abs(Rgba[Q * 4 + 3] - Byte(O.Colour.w));
				M.MostColour = std::max(M.MostColour, Colour);
				M.MostAlpha = std::max(M.MostAlpha, Alpha);
				if (Colour > ColourGive) ++M.ColourOff;
				if (Alpha > AlphaGive) ++M.AlphaOff;
				const double Sheet = std::max({ std::fabs(Uvs[Q * 4] - O.Sheet.x), std::fabs(Uvs[Q * 4 + 1] - O.Sheet.y), std::fabs(Uvs[Q * 4 + 2] - O.Sheet.z), std::fabs(Uvs[Q * 4 + 3] - O.Sheet.w) });
				M.MostSheet = std::max(M.MostSheet, Sheet);
				if (Sheet > SheetGive) ++M.SheetOff;
			}
			++I;
			++J;
		}
		M.PainterOnly += PainterN - I;
		M.ChipOnly += ChipN - J;
		return M;
	}

	/** The table laid at one eye and read at another (the chip's frames between two lays), against a table laid there:
	 * puffs drawn by one and not the other, and points that differ. */
	struct Stale
	{
		int Eyes = 0, Missing = 0, Extra = 0;
		long Points = 0, Differ = 0;
		void Add(const Stale& O)
		{
			Eyes += O.Eyes;
			Missing += O.Missing;
			Extra += O.Extra;
			Points += O.Points;
			Differ += O.Differ;
		}
	};

	Stale ReadElsewhere(const AirSky::Field& F, const Vec& Eye, const Vec& Moved, const Vec& Drift, const Vec& Sun, const AirSky::Rgb& Light, bool bPointsToo)
	{
		Stale Out;
		AirSky::Chip Then, Now;
		Then.Lay(F, Eye, Drift, Sun, Light);
		Now.Lay(F, Moved, Drift, Sun, Light);
		const ChipReader ReadThen(Then), ReadNow(Now);
		const Vec Seen = Moved - Drift;
		std::map<int, int> ThenDrawn, NowDrawn;
		for (int S = 0; S < Then.Laid(); ++S)
			if (ReadThen.Drawn(S, Seen)) ThenDrawn[Then.SlotPuff(S)] = S;
		for (int S = 0; S < Now.Laid(); ++S)
			if (ReadNow.Drawn(S, Seen)) NowDrawn[Now.SlotPuff(S)] = S;
		++Out.Eyes;
		for (const auto& Each : NowDrawn)
		{
			const auto Found = ThenDrawn.find(Each.first);
			if (Found == ThenDrawn.end())
			{
				++Out.Missing;
				continue;
			}
			for (int P = 0; bPointsToo && P < Then.PerPuff(); ++P)
			{
				const AirSkyChipCpp::FSkyChipOut A = ReadThen.At(Found->second, P, Seen), B = ReadNow.At(Each.second, P, Seen);
				++Out.Points;
				const double D = std::max({ std::fabs(A.At.x - B.At.x), std::fabs(A.At.y - B.At.y), std::fabs(A.At.z - B.At.z) }) +
					1000.0 * std::max({ std::fabs(A.Colour.x - B.Colour.x), std::fabs(A.Colour.y - B.Colour.y), std::fabs(A.Colour.z - B.Colour.z), std::fabs(A.Colour.w - B.Colour.w),
						std::fabs(A.Sheet.x - B.Sheet.x), std::fabs(A.Sheet.y - B.Sheet.y), std::fabs(A.Sheet.z - B.Sheet.z), std::fabs(A.Sheet.w - B.Sheet.w) });
				if (D > 1e-3) ++Out.Differ;
			}
		}
		for (const auto& Each : ThenDrawn)
			if (!NowDrawn.count(Each.first)) ++Out.Extra;
		return Out;
	}

	/** A painter of the shadows alone: it owns none of the clouds (its part takes none), so it lays only their shadows
	 * on the sea and the land, every one the far room lays, as loose as the far room (its turn by AirSky::Rooms's
	 * own measure). A puff's room of one, never used. */
	AirSky::Painter ShadowsAlone(double Ink)
	{
		AirSky::Painter Shade;
		AirSky::Part None;
		None.bAll = false;
		None.bNear = false;
		None.Split = 1e18;
		Shade.SetPart(None);
		Shade.SetRoom(1);
		Shade.SetInk(Ink);
		Shade.SetLoose(0.0005, 0.04);
		return Shade;
	}

	double LeastMiddle(const std::function<void()>& Work)
	{
		// (the middle of 15, the least of three tries: one slow stretch of a shared machine is not the work's)
		double Least = 1e18;
		for (int Try = 0; Try < 3; ++Try)
		{
			std::vector<double> Took;
			for (int Rep = 0; Rep < 15; ++Rep)
			{
				const auto T0 = Clock::now();
				Work();
				Took.push_back(Ms(T0));
			}
			std::nth_element(Took.begin(), Took.begin() + 7, Took.end());
			Least = std::min(Least, Took[7]);
		}
		return Least;
	}

	/** One sky to check: a field, its sun and light, the eyes it is looked at from, the carry. */
	struct Case
	{
		std::string Name;
		AirSky::Field F;
		Vec Sun;
		AirSky::Rgb Light = AirSky::Rgb(1.0f, 1.0f, 1.0f);
		double Ink = 1.3;
		std::vector<Vec> Eyes;
		Vec Drift;
	};

	/** The ground battle's sky as LandBattle::LaySky lays it: a summer cumulus over the field (or a deck), the air
	 * battle's clearing 35 km off, no ground under the clouds. */
	Case GroundCase(bool bDeck)
	{
		Case C;
		C.Name = bDeck ? "ground battle, a deck" : "ground battle, cumulus";
		AirSky::Day D;
		D.bGiven = true;
		D.bDeck = bDeck;
		D.Base = bDeck ? 1500.0 : 900.0;
		D.Top = D.Base + (bDeck ? 600.0 : 1100.0);
		const double W = 2400.0, H = 2400.0;
		AirSky::Placing P;
		P.Seed = 238;
		P.TargetX = 0.5 * W + 25000.0;
		P.TargetY = 0.5 * H + 25000.0;
		P.Band = 0.0;
		P.LowBand = 0.0;
		P.bShips = false;
		C.Sun = Vec(0.42, -0.55, 0.72).Unit();
		P.Sun = C.Sun;
		C.F = AirSky::Lay(D, P);
		C.Light = bDeck ? AirSky::Rgb(0.82f, 0.83f, 0.86f) : AirSky::Rgb(1.0f, 1.0f, 1.0f);
		C.Drift = C.F.DriftAt(240.0);
		C.Eyes = { Vec(1200.0, 1200.0, 2.5), Vec(400.0, 1800.0, 30.0), Vec(1500.0, 300.0, 400.0), Vec(2600.0, 2400.0, 1800.0) };
		for (Vec& E : C.Eyes) E += C.Drift;
		return C;
	}
}

int main(int Argc, char** Argv)
{
	std::vector<Case> Cases;
	Cases.push_back(GroundCase(false));
	Cases.push_back(GroundCase(true));
	for (int A = 1; A < Argc; ++A)
	{
		std::ifstream In(Argv[A], std::ios::binary);
		std::stringstream Buffer;
		Buffer << In.rdbuf();
		AirFeed::Recording Rec;
		std::string Error;
		if (!In || !AirFeed::ReadRecording(Buffer.str(), Rec, Error))
		{
			std::fprintf(stderr, "cannot read %s: %s\n", Argv[A], Error.c_str());
			return 2;
		}
		Case C;
		C.Name = Argv[A];
		C.Name = C.Name.substr(C.Name.find_last_of('/') + 1);
		AirSky::Placing P = AirFeed::PlacingOf(Rec.Board, Rec.Views.front().Groups, Rec.Seed);
		// (the board's own land under a battle with no stage, as the cloud test lays it: the clouds lifted and eased over it)
		const std::shared_ptr<const AirGround::BoardLand> Land =
			Rec.Board.StageKey.empty() ? std::make_shared<const AirGround::BoardLand>(Rec.Board.Land) : std::shared_ptr<const AirGround::BoardLand>();
		if (Land && !Land->IsEmpty())
		{
			P.Ground = [Land](double X, double Y) { return Land->At(X, Y); };
			P.GroundTop = AirGround::BoardLand::LandHigh;
		}
		C.F = AirSky::Lay(Rec.Board.Cloud, P);
		C.Sun = P.Sun;
		C.Drift = C.F.DriftAt(600.0);
		const double TX = Rec.Board.TargetX, TY = Rec.Board.TargetY;
		C.Eyes = { Vec(TX, TY - 6000.0, 5000.0), Vec(TX + 3000.0, TY, 1200.0), Vec(TX - 9000.0, TY + 4000.0, 300.0), Vec(TX, TY, 9000.0) };
		Cases.push_back(C);
	}

	std::vector<Limit> Limits;
	Match Whole;
	Stale Within, Margin;
	double WorstLayMs = 0.0, WorstAskUs = 0.0, WorstShadeMs = 0.0, ShadowMost = 0.0;
	int ShadowEyes = 0, ShadowDiffer = 0;
	long ShadowsLaid = 0;
	int MostLaid = 0, MostDropped = 0, LeastSlackEye = 1 << 30;
	double LeastSlack = 1e18;
	for (const Case& C : Cases)
	{
		Match Here;
		AirSky::Chip Ch;
		for (const Vec& E : C.Eyes)
		{
			const Match M = AgainstPainter(C.F, E, C.Drift, C.Sun, C.Light, C.Ink, Ch);
			Here.Add(M);
			MostLaid = std::max(MostLaid, Ch.Laid());
			MostDropped = std::max(MostDropped, Ch.Dropped());
			if (Ch.Slack() < LeastSlack)
			{
				LeastSlack = Ch.Slack();
				LeastSlackEye = static_cast<int>(&E - &C.Eyes.front());
			}
			// (read elsewhere: eight ways out at 95 per cent of the slack, points and all; and at 95 per cent of the Margin,
			// the puffs drawn only)
			for (int Way = 0; Way < 8; ++Way)
			{
				const double A = Way * 0.785398, Up = Way % 3 == 0 ? 0.3 : Way % 3 == 1 ? -0.2 : 0.0;
				const Vec Dir = Vec(std::cos(A), std::sin(A), Up).Unit();
				Vec Near = E + Dir * (0.95 * Ch.Slack()), Far = E + Dir * (0.95 * AirSky::Chip::Margin);
				Near.Z = std::max(Near.Z, 1.0);
				Far.Z = std::max(Far.Z, 1.0);
				Within.Add(ReadElsewhere(C.F, E, Near, C.Drift, C.Sun, C.Light, true));
				Margin.Add(ReadElsewhere(C.F, E, Far, C.Drift, C.Sun, C.Light, false));
			}
			// (what a lay costs, and the frame's ask)
			AirSky::Chip Timed;
			Timed.Lay(C.F, E, C.Drift, C.Sun, C.Light);
			WorstLayMs = std::max(WorstLayMs, LeastMiddle([&]() { Timed.Lay(C.F, E, C.Drift, C.Sun, C.Light); }));
			volatile bool bSink = false;
			const auto T0 = Clock::now();
			for (int Rep = 0; Rep < 100000; ++Rep) bSink = bSink ^ Timed.Wants(E + Vec(0.0, 0.0, Rep * 1e-6), C.Drift);
			WorstAskUs = std::max(WorstAskUs, Ms(T0) * 1000.0 / 100000.0);
			// (the shadows on the sea, painted alone by a painter that owns none of the clouds, as a scene does whose
			// clouds the chip draws: the far room's own shadows, point for point, and what that costs)
			AirSky::Rooms Today;
			Today.SetSprites(true);
			Today.SetInk(C.Ink);
			Today.PaintBoth(C.F, E, C.Drift, C.Sun, C.Light);
			AirSky::Painter Shade = ShadowsAlone(C.Ink);
			std::vector<Vec> NoPoints, ShadowPoints;
			std::vector<uint8_t> NoRgba, ShadowRgba;
			Shade.Paint(C.F, E, C.Drift, C.Sun, C.Light, NoPoints, NoRgba, &ShadowPoints, &ShadowRgba, nullptr);
			++ShadowEyes;
			if (ShadowPoints.size() != Today.Shadows.size() || ShadowRgba != Today.ShadowRgba)
			{
				++ShadowDiffer;
			}
			else
			{
				for (size_t Q = 0; Q < ShadowPoints.size(); ++Q)
				{
					ShadowMost = std::max(ShadowMost, (ShadowPoints[Q] - Today.Shadows[Q]).Length());
				}
				for (size_t Q = 3; Q < ShadowRgba.size(); Q += 4)
				{
					ShadowsLaid += ShadowRgba[Q] > 0 ? 1 : 0;
				}
			}
			WorstShadeMs = std::max(WorstShadeMs, LeastMiddle([&]() { Shade.Paint(C.F, E, C.Drift, C.Sun, C.Light, NoPoints, NoRgba, &ShadowPoints, &ShadowRgba, nullptr); }));
		}
		std::printf("%-26s %d puffs drawn and matched, %ld points: place %.3f m at most (%ld over %.2f), colour %d (%ld over %d), alpha %d (%ld over %d), sheet %.4f (%ld over %.3f); %d drawn by the painter only, %d by the chip only\n",
		            C.Name.c_str(), Here.Puffs, Here.Points, Here.MostPlace, Here.PlaceOff, PlaceGive, Here.MostColour, Here.ColourOff, ColourGive, Here.MostAlpha, Here.AlphaOff, AlphaGive, Here.MostSheet,
		            Here.SheetOff, SheetGive, Here.PainterOnly, Here.ChipOnly);
		Whole.Add(Here);
	}
	(void)LeastSlackEye;

	// The ground battle's eye through 30 s of shots at 60 frames a second, the wind carrying the clouds: the rooms (today's
	// way) against the chip.
	struct PathRun
	{
		int NearTurns = 0, FarTurns = 0, Lays = 0;
		double TurnMs = 0.0, MostTurnMs = 0.0, LayMs = 0.0, MostLayMs = 0.0;
		long PointsSent = 0;
		double BytesSent = 0.0;
	};
	std::vector<std::pair<std::string, PathRun>> Paths;
	for (int CaseIndex = 0; CaseIndex < 2; ++CaseIndex)
	{
		const Case& C = Cases[static_cast<size_t>(CaseIndex)];
		AirSky::Rooms Two;
		Two.SetSprites(true);
		Two.SetInk(C.Ink);
		AirSky::Chip Ch;
		Ch.SetInk(C.Ink);
		PathRun Run;
		auto EyeAt = [](double T)
		{
			// (a dolly along the field at 6 m/s; a crane up to 60 m; a cut to a slow orbit of 40 m; a cut to the air, 25 m/s)
			if (T < 8.0) return Vec(700.0 + 6.0 * T, 1200.0, 2.5);
			if (T < 12.0) return Vec(748.0 + 3.0 * (T - 8.0), 1200.0, 2.5 + 57.5 * (T - 8.0) / 4.0);
			if (T < 22.0) return Vec(500.0 + 40.0 * std::cos(0.3 * (T - 12.0)), 1700.0 + 40.0 * std::sin(0.3 * (T - 12.0)), 15.0);
			return Vec(1200.0, 400.0 + 25.0 * (T - 22.0), 350.0);
		};
		const double Start = 240.0;
		for (int Frame = 0; Frame < 30 * 60; ++Frame)
		{
			const double T = Frame / 60.0;
			const Vec Drift = C.F.DriftAt(Start + T);
			const Vec E = EyeAt(T) + Drift;
			auto T0 = Clock::now();
			const AirSky::Rooms::Sent What = Two.Turn(C.F, E, Drift, C.Sun, C.Light);
			const double Took = Ms(T0);
			if (What != AirSky::Rooms::Sent::None)
			{
				Run.TurnMs += Took;
				Run.MostTurnMs = std::max(Run.MostTurnMs, Took);
				if (What == AirSky::Rooms::Sent::Both)
				{
					++Run.FarTurns;
					Run.PointsSent += static_cast<long>(Two.NearPoints.size() + Two.FarPoints.size());
				}
				else
				{
					++Run.NearTurns;
					Run.PointsSent += static_cast<long>(Two.NearPoints.size());
				}
			}
			T0 = Clock::now();
			if (Ch.Wants(E, Drift))
			{
				Ch.Lay(C.F, E, Drift, C.Sun, C.Light);
				const double Laid = Ms(T0);
				++Run.Lays;
				Run.LayMs += Laid;
				Run.MostLayMs = std::max(Run.MostLayMs, Laid);
				Run.BytesSent += static_cast<double>(Ch.RowsUsed()) * AirSky::Chip::Wide * 16.0;
			}
		}
		Paths.push_back({ C.Name, Run });
		const int Turns = std::max(1, Run.NearTurns + Run.FarTurns);
		std::printf("%-26s 30 s of shots: rooms %d near turns and %d far, %.2f ms a turn (%.2f at most), %ld points sent (%.0f a turn, %.1f MB); chip %d lays, %.2f ms a lay (%.2f at most), %.2f MB of table sent, no points\n",
		            C.Name.c_str(), Run.NearTurns, Run.FarTurns, Run.TurnMs / Turns, Run.MostTurnMs, Run.PointsSent, static_cast<double>(Run.PointsSent) / Turns,
		            Run.PointsSent * 36.0 / 1e6, Run.Lays, Run.Lays ? Run.LayMs / Run.Lays : 0.0, Run.MostLayMs, Run.BytesSent / 1e6);
	}

	const long Over = Whole.PlaceOff + Whole.ColourOff + Whole.AlphaOff + Whole.SheetOff;
	Limits.push_back({ "the chip's points are the painter's", Whole.Puffs > 0 && Over <= Whole.Points / 1000,
	                   std::to_string(Whole.Puffs) + " puffs, " + std::to_string(Whole.Points) + " points: place " + Num(Whole.MostPlace, 3) + " m at most, colour " + std::to_string(Whole.MostColour) +
	                       ", alpha " + std::to_string(Whole.MostAlpha) + ", sheet " + Num(Whole.MostSheet, 4) + "; " + std::to_string(Over) + " points past the give",
	                   "a point in a thousand at most past " + Num(PlaceGive) + " m, " + std::to_string(ColourGive) + " of colour or alpha in 255, " + Num(SheetGive, 3) + " on the sheet" });
	Limits.push_back({ "the chip draws the painter's puffs", Whole.PainterOnly + Whole.ChipOnly <= std::max(2, Whole.Puffs / 500),
	                   std::to_string(Whole.PainterOnly) + " by the painter only, " + std::to_string(Whole.ChipOnly) + " by the chip only, of " + std::to_string(Whole.Puffs),
	                   "a puff in 500 at most, each at the edge of being drawn" });
	Limits.push_back({ "a table read within its slack is the table laid there", Within.Missing == 0 && Within.Extra == 0 && Within.Differ == 0,
	                   std::to_string(Within.Eyes) + " eyes: " + std::to_string(Within.Missing) + " puffs missing, " + std::to_string(Within.Extra) + " extra, " + std::to_string(Within.Differ) + " of " +
	                       std::to_string(Within.Points) + " points differ",
	                   "none (the chip's frames between two lays draw what a lay there would)" });
	Limits.push_back({ "a table read at its margin holds every puff drawn there", Margin.Missing == 0,
	                   std::to_string(Margin.Eyes) + " eyes " + Num(0.95 * AirSky::Chip::Margin, 0) + " m off: " + std::to_string(Margin.Missing) + " puffs missing", "none" });
	Limits.push_back({ "the chip's table has room", MostDropped == 0, std::to_string(MostLaid) + " puffs at most of " + std::to_string(AirSky::Chip().Room()) + ", " + std::to_string(MostDropped) + " left out",
	                   "none left out" });
	Limits.push_back({ "the chip's table laid within budget", WorstLayMs < 2.5, Num(WorstLayMs, 2) + " ms", "under 2.5 ms (the least of three middles of 15; the painter's turn is under 5)" });
	Limits.push_back({ "the shadows painted alone are the far room's", ShadowEyes > 0 && ShadowDiffer == 0 && ShadowMost < 1e-6,
	                   std::to_string(ShadowEyes) + " eyes, " + std::to_string(ShadowDiffer) + " differ, " + std::to_string(ShadowsLaid) + " shadow points shown, " + Num(ShadowMost, 6) + " m at most apart",
	                   "the same points and paints" });
	Limits.push_back({ "the shadows painted alone within budget", WorstShadeMs < 1.5, Num(WorstShadeMs, 2) + " ms", "under 1.5 ms (the least of three middles of 15; on the game's thread, about twice a second)" });
	Limits.push_back({ "the chip's frame ask within budget", WorstAskUs < 2.0, Num(WorstAskUs, 3) + " us", "under 2 microseconds" });
	for (const auto& Each : Paths)
	{
		const PathRun& R = Each.second;
		Limits.push_back({ "the ground's 30 s of shots: the chip lays less than the rooms turn (" + Each.first + ")", R.Lays * 3 <= R.NearTurns + R.FarTurns,
		                   std::to_string(R.Lays) + " lays against " + std::to_string(R.NearTurns + R.FarTurns) + " turns", "a third as many or fewer" });
	}
	int Failed = 0;
	for (const Limit& L : Limits)
	{
		std::printf("  %s %-62s %s (%s)\n", L.bOk ? "ok  " : "FAIL", L.What.c_str(), L.Got.c_str(), L.Want.c_str());
		Failed += L.bOk ? 0 : 1;
	}
	std::printf(Failed ? "%d CHECKS FAILED\n" : "ALL CHECKS PASSED\n", Failed);
	return Failed ? 1 : 0;
}
