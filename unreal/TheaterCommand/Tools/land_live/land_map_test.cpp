// The map of the field's cloud test (release 2; Brian, 26 November 2026: "a small map of the battle on the bottom right
// to know where everyone is"). Battle/LandFieldMap is plain C++, so what the screen will rely on is held here:
//  - the map stands where the UI director's marks put it (208 on its longer side, the field's shape, its foot 10 up,
//    its right edge 18 in), and a place on the field falls on the map and comes back from it, turned half round or not;
//  - the field's woods, water and buildings are painted where they lie, a river one cell wide runs from edge to edge
//    and is broken only where a bridge crosses it (the bridge's square is plain ground), the runs never overlap or
//    leave the map, and the same ground is painted the same on a map turned round;
//  - a unit's mark runs along its frontage at its facing and is never smaller than 5 by 3;
//  - the outline of what the view sees is cut to the map's box and closed, and is nothing where it falls off the map.
// Usage: land_map_test   (no files: the field is made here)

#include "Battle/LandFieldMap.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace
{
	int Failures = 0;

	void Check(bool bOk, const std::string& What)
	{
		if (bOk) return;
		++Failures;
		if (Failures <= 30) std::printf("FAIL: %s\n", What.c_str());
	}

	bool Near(double A, double B, double Slack = 1e-6) { return std::fabs(A - B) <= Slack; }

	/** What the map paints at a place on the field (the run that holds its square; plain ground where none does). */
	LandFieldMap::Paint PaintAt(const LandFieldMap::Painted& Of, const LandFieldMap::Frame& In, double FieldX, double FieldY)
	{
		double Mx = 0.0, My = 0.0;
		In.ToMap(FieldX, FieldY, Mx, My);
		const int Col = static_cast<int>(std::floor((Mx - In.X) / Of.Square)), Row = static_cast<int>(std::floor((My - In.Y) / Of.Square));
		for (const LandFieldMap::Run& One : Of.Runs)
		{
			if (One.Row == Row && Col >= One.Col && Col < One.Col + One.Count) return One.What;
		}
		return LandFieldMap::Paint::Ground;
	}
}

int main()
{
	using namespace LandFieldMap;

	// ---- Where it stands. ----
	const Frame Wide = Lay(1920.0, 1080.0, 3000.0, 1920.0, false);
	Check(Wide.Valid() && Near(Wide.W, 208.0) && Near(Wide.H, 208.0 * 1920.0 / 3000.0), "a field wider than tall: 208 wide, the field's shape");
	Check(Near(Wide.X + Wide.W, 1920.0 - 18.0) && Near(Wide.Y + Wide.H, 1080.0 - 10.0), "its right edge 18 in, its foot 10 up");
	Check(Near(Wide.LeftOf(), Wide.X - 10.0), "the foot's buttons end 10 clear of its left edge");
	const Frame Tall = Lay(1920.0, 1080.0, 1500.0, 3000.0, false);
	Check(Tall.Valid() && Near(Tall.H, 208.0) && Near(Tall.W, 104.0), "a field taller than wide: never taller than 208");
	Check(!Lay(0.0, 1080.0, 3000.0, 1920.0, false).Valid() && !Lay(1920.0, 1080.0, 0.0, 1920.0, false).Valid(), "no screen or no field: no map");
	Check(Near(Wide.Scale(), 208.0 / 3000.0) && Near(Wide.H / Wide.FieldH, Wide.Scale()), "one scale across and up");

	// ---- A place on the field falls on the map, and comes back. ----
	const Frame Turned = Lay(1920.0, 1080.0, 3000.0, 1920.0, true);
	{
		double Mx = 0.0, My = 0.0;
		Wide.ToMap(0.0, 0.0, Mx, My);
		Check(Near(Mx, Wide.X) && Near(My, Wide.Y + Wide.H), "north up: the south-west corner at the map's bottom left");
		Wide.ToMap(3000.0, 1920.0, Mx, My);
		Check(Near(Mx, Wide.X + Wide.W) && Near(My, Wide.Y), "north up: the north-east corner at the map's top right");
		Turned.ToMap(0.0, 0.0, Mx, My);
		Check(Near(Mx, Turned.X + Turned.W) && Near(My, Turned.Y), "turned: the south-west corner at the map's top right");
		Turned.ToMap(1500.0, 1920.0, Mx, My);
		Check(Near(Mx, Turned.X + Turned.W * 0.5) && Near(My, Turned.Y + Turned.H), "turned: the north edge, the defence's own ground, at the map's foot");
		for (const Frame& In : { Wide, Turned, Tall })
		{
			for (int K = 0; K < 200; ++K)
			{
				const double Fx = In.FieldW * ((K * 37) % 200) / 199.0, Fy = In.FieldH * ((K * 91) % 200) / 199.0;
				double Bx = 0.0, By = 0.0;
				In.ToMap(Fx, Fy, Mx, My);
				Check(In.Holds(Mx, My), "a place on the field falls on the map's box");
				Check(In.ToField(Mx, My, Bx, By) && Near(Bx, Fx, 1e-6) && Near(By, Fy, 1e-6), "a place comes back from the map as it went");
			}
			double Bx = 0.0, By = 0.0;
			Check(!In.ToField(In.X - 40.0, In.Y + 5.0, Bx, By) && Bx >= 0.0 && Bx <= In.FieldW && By >= 0.0 && By <= In.FieldH, "a point off the map is said so, and held to the field");
		}
	}

	// ---- The field painted small. Cells of 25 m, 120 by 80 (3000 by 2000), row 0 the south edge: a wood, a river one
	// cell wide from south to north, a village, and one house standing alone on open ground (its storeys only). ----
	LandFeed::Field F;
	F.W = 3000.0;
	F.H = 2000.0;
	F.Cell = 25.0;
	F.Cols = 120;
	F.Rows = 80;
	F.Grounds = { "open", "woods", "water", "buildings", "road", "marsh", "bridge", "blown" };
	F.Ground.assign(static_cast<size_t>(F.Cols) * static_cast<size_t>(F.Rows), 0);
	F.Storeys.assign(F.Ground.size(), 0);
	auto Set = [&F](int Col, int Row, std::uint8_t Kind) { F.Ground[static_cast<size_t>(Row) * static_cast<size_t>(F.Cols) + static_cast<size_t>(Col)] = Kind; };
	for (int Row = 10; Row < 30; ++Row) for (int Col = 10; Col < 30; ++Col) Set(Col, Row, 1);
	for (int Row = 0; Row < F.Rows; ++Row) Set(60, Row, 2);
	for (int Row = 40; Row < 46; ++Row) for (int Col = 100; Col < 106; ++Col) Set(Col, Row, 3);
	for (int Col = 0; Col < F.Cols; ++Col) if (Col != 60) Set(Col, 70, 4);
	// (one bridge over the river, at 500 to 525 north, and one blown, at 1250 to 1275)
	Set(60, 20, 6);
	Set(60, 50, 7);
	F.Storeys[static_cast<size_t>(60) * static_cast<size_t>(F.Cols) + 40] = 2;
	for (const bool bTurn : { false, true })
	{
		const Frame In = Lay(1920.0, 1080.0, F.W, F.H, bTurn);
		const Painted P = Paints(F, In);
		const std::string Which = bTurn ? " (turned)" : " (north up)";
		Check(P.Cols == static_cast<int>(std::ceil(In.W / 3.0 - 1e-9)) && P.Rows == static_cast<int>(std::ceil(In.H / 3.0 - 1e-9)) && Near(P.Square, 3.0), "squares of 3 over the whole map" + Which);
		Check(!P.Runs.empty() && P.Runs.size() < static_cast<size_t>(P.Rows) * 8, "a few runs a row, not a square each" + Which);
		// (the runs: in the map, in order, never overlapping, never plain ground)
		int LastRow = -1, LastEnd = 0;
		for (const Run& One : P.Runs)
		{
			Check(One.Count > 0 && One.Row >= 0 && One.Row < P.Rows && One.Col >= 0 && One.Col + One.Count <= P.Cols, "a run lies on the map" + Which);
			Check(One.What != Paint::Ground, "plain ground is not listed" + Which);
			Check(One.Row > LastRow || (One.Row == LastRow && One.Col >= LastEnd), "runs in order, none over another" + Which);
			LastEnd = One.Row == LastRow || One.Row > LastRow ? One.Col + One.Count : LastEnd;
			LastRow = One.Row;
			double X = 0.0, Y = 0.0, W = 0.0, H = 0.0;
			RunBox(P, In, One, X, Y, W, H);
			Check(W > 0.0 && H > 0.0 && X >= In.X - 1e-9 && Y >= In.Y - 1e-9 && X + W <= In.X + In.W + 1e-9 && Y + H <= In.Y + In.H + 1e-9, "a run's box is cut to the map's box" + Which);
		}
		// (each thing where it lies)
		Check(PaintAt(P, In, 500.0, 500.0) == Paint::Woods, "the wood's middle is woods" + Which);
		Check(PaintAt(P, In, 2575.0, 1075.0) == Paint::Building, "the village is buildings" + Which);
		Check(PaintAt(P, In, 1012.5, 1512.5) == Paint::Building, "a house alone on open ground is a building (its storeys)" + Which);
		Check(PaintAt(P, In, 2000.0, 400.0) == Paint::Ground && PaintAt(P, In, 200.0, 1900.0) == Paint::Ground, "open ground is the map's own plate" + Which);
		Check(PaintAt(P, In, 2000.0, 1762.5) == Paint::Ground, "a road is not drawn" + Which);
		// (the river, one cell wide: water in every row of the map but the bridge's, so it runs from edge to edge and
		// the one crossing shows as a gap in it, a square or two long)
		int Dry = 0;
		for (int Row = 0; Row < P.Rows; ++Row)
		{
			bool bWet = false;
			for (const Run& One : P.Runs) bWet = bWet || (One.Row == Row && One.What == Paint::Water);
			Dry += bWet ? 0 : 1;
		}
		Check(Dry >= 1 && Dry <= 2, "the river is in every row of the map but the bridge's one or two" + Which);
		Check(PaintAt(P, In, 1512.5, 1000.0) == Paint::Water, "the river where it lies" + Which);
		Check(PaintAt(P, In, 1512.5, 512.5) == Paint::Ground, "a bridge wins over the water under its square: plain ground" + Which);
		Check(PaintAt(P, In, 1512.5, 400.0) == Paint::Water && PaintAt(P, In, 1512.5, 625.0) == Paint::Water, "the river runs on each side of its bridge" + Which);
		Check(PaintAt(P, In, 1512.5, 1262.5) == Paint::Water, "a blown bridge is river again" + Which);
	}
	Check(Paints(F, Frame()).Runs.empty() && Paints(LandFeed::Field(), Wide).Runs.empty(), "no frame or no field: nothing painted");

	// ---- A unit's mark. ----
	{
		const double Pi = 3.14159265358979323846;
		double Ax = 0.0, Ay = 0.0, Bx = 0.0, By = 0.0, Thick = 0.0, Cx = 0.0, Cy = 0.0;
		Wide.ToMap(1500.0, 300.0, Cx, Cy);
		Mark(Wide, 1500.0, 300.0, 300.0, 40.0, 0.0, Ax, Ay, Bx, By, Thick);
		Check(Near(Ay, By) && Near(std::fabs(Bx - Ax), 300.0 * Wide.Scale()) && Near((Ax + Bx) * 0.5, Cx) && Near(Ay, Cy), "facing north: its frontage across the map, about its place");
		Check(Near(Thick, 3.0), "never thinner than 3");
		Mark(Wide, 1500.0, 300.0, 300.0, 40.0, Pi * 0.5, Ax, Ay, Bx, By, Thick);
		Check(Near(Ax, Bx, 1e-6) && Near(std::fabs(By - Ay), 300.0 * Wide.Scale()), "facing east: its frontage up and down the map");
		Mark(Wide, 1500.0, 300.0, 20.0, 12.0, 0.7, Ax, Ay, Bx, By, Thick);
		Check(Near(std::hypot(Bx - Ax, By - Ay), 5.0) && Near(Thick, 3.0), "a small unit is still 5 by 3");
		Mark(Wide, 1500.0, 300.0, 600.0, 120.0, Pi / 6.0, Ax, Ay, Bx, By, Thick);
		Check(Near(std::hypot(Bx - Ax, By - Ay), 600.0 * Wide.Scale()) && Near(Thick, 120.0 * Wide.Scale()), "a large unit at its real frontage and depth");
		// (the stroke turns with the map: the same unit on a map turned half round lies along the same line)
		double Tax = 0.0, Tay = 0.0, Tbx = 0.0, Tby = 0.0, TThick = 0.0;
		Mark(Turned, 1500.0, 300.0, 600.0, 120.0, Pi / 6.0, Tax, Tay, Tbx, Tby, TThick);
		Check(Near((Bx - Ax) * (Tby - Tay) - (By - Ay) * (Tbx - Tax), 0.0, 1e-6) && Near(TThick, Thick), "turned half round: the same line");
		// (north-east of north by 30 degrees: its right end is east and south of its left; on the map east is right and south is down)
		Check((Bx - Ax) * (By - Ay) > 0.0, "a unit facing a little east of north leans the way the field shows it");
	}

	// ---- The outline of what the view sees. ----
	{
		const std::vector<std::pair<double, double>> Inside = { { 1000.0, 400.0 }, { 2000.0, 400.0 }, { 2300.0, 1200.0 }, { 700.0, 1200.0 } };
		const std::vector<std::pair<double, double>> Whole = Outline(Wide, Inside);
		Check(Whole.size() == 5 && Near(Whole.front().first, Whole.back().first) && Near(Whole.front().second, Whole.back().second), "a view inside the field: its four corners, closed");
		for (size_t K = 0; K < Inside.size() && K < Whole.size(); ++K)
		{
			double Mx = 0.0, My = 0.0;
			Wide.ToMap(Inside[K].first, Inside[K].second, Mx, My);
			Check(Near(Whole[K].first, Mx) && Near(Whole[K].second, My), "each corner where the field puts it");
		}
		const std::vector<std::pair<double, double>> Partly = Outline(Wide, { { 2500.0, 1500.0 }, { 3600.0, 1500.0 }, { 5200.0, 4000.0 }, { 1800.0, 4000.0 } });
		Check(Partly.size() >= 4, "a view running off the field is cut, not dropped");
		for (const std::pair<double, double>& P : Partly)
		{
			Check(P.first >= Wide.X - 1e-6 && P.first <= Wide.X + Wide.W + 1e-6 && P.second >= Wide.Y - 1e-6 && P.second <= Wide.Y + Wide.H + 1e-6, "no point of the outline leaves the map's box");
		}
		Check(Outline(Wide, { { 4000.0, 100.0 }, { 5000.0, 100.0 }, { 5000.0, 900.0 }, { 4000.0, 900.0 } }).empty(), "a view wholly off the field: no outline");
		const std::vector<std::pair<double, double>> Over = Outline(Wide, { { -9000.0, -9000.0 }, { 12000.0, -9000.0 }, { 12000.0, 11000.0 }, { -9000.0, 11000.0 } });
		Check(Over.size() == 5, "a view of the whole field: the map's own box");
		Check(Outline(Wide, { { 100.0, 100.0 }, { 200.0, 200.0 } }).empty() && Outline(Frame(), Inside).empty(), "fewer than three corners, or no map: no outline");
		const std::vector<std::pair<double, double>> Round = Outline(Turned, Inside);
		Check(Round.size() == 5, "turned half round: still its four corners");
	}

	if (Failures == 0) std::printf("PASS: the map of the field (frame, places, paint, marks, outline)\n");
	else std::printf("FAIL: %d checks\n", Failures);
	return Failures == 0 ? 0 : 1;
}
