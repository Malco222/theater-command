#include "Battle/LandFieldMap.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>

namespace LandFieldMap
{
	void Frame::ToMap(double FieldX, double FieldY, double& OutX, double& OutY) const
	{
		const double Across = FieldW > 0.0 ? FieldX / FieldW : 0.0, Up = FieldH > 0.0 ? FieldY / FieldH : 0.0;
		// (north up: the field's y runs up the map; turned half round, both run the other way)
		OutX = X + (bTurned ? 1.0 - Across : Across) * W;
		OutY = Y + (bTurned ? Up : 1.0 - Up) * H;
	}

	bool Frame::ToField(double MapX, double MapY, double& OutX, double& OutY) const
	{
		if (!Valid())
		{
			OutX = OutY = 0.0;
			return false;
		}
		const double Across = std::clamp((MapX - X) / W, 0.0, 1.0), Down = std::clamp((MapY - Y) / H, 0.0, 1.0);
		OutX = (bTurned ? 1.0 - Across : Across) * FieldW;
		OutY = (bTurned ? Down : 1.0 - Down) * FieldH;
		return Holds(MapX, MapY);
	}

	Frame Lay(double ScreenW, double ScreenH, double FieldW, double FieldH, bool bTurned)
	{
		Frame Out;
		if (ScreenW <= 0.0 || ScreenH <= 0.0 || FieldW <= 0.0 || FieldH <= 0.0)
		{
			return Out;
		}
		// (the field's shape: its longer side takes the map's whole side, so it is never wider or taller than that)
		const double Scale = Side / std::max(FieldW, FieldH);
		Out.W = FieldW * Scale;
		Out.H = FieldH * Scale;
		Out.X = ScreenW - RightIn - Out.W;
		Out.Y = ScreenH - Foot - Out.H;
		Out.FieldW = FieldW;
		Out.FieldH = FieldH;
		Out.bTurned = bTurned;
		return Out;
	}

	Painted Paints(const LandFeed::Field& Of, const Frame& In, double Square)
	{
		Painted Out;
		const std::size_t Cells = static_cast<std::size_t>(std::max(Of.Cols, 0)) * static_cast<std::size_t>(std::max(Of.Rows, 0));
		if (!In.Valid() || Square <= 0.0 || Of.Cell <= 0.0 || Cells == 0 || Of.Ground.size() < Cells)
		{
			return Out;
		}
		Out.Square = Square;
		Out.Cols = static_cast<int>(std::ceil(In.W / Square - 1e-9));
		Out.Rows = static_cast<int>(std::ceil(In.H / Square - 1e-9));
		// (each of the field's ground words once: what it is painted)
		std::vector<Paint> Words(Of.Grounds.size(), Paint::Ground);
		// (and which word is a bridge: a crossing is shown on its river)
		std::vector<bool> Crossing(Of.Grounds.size(), false);
		for (std::size_t K = 0; K < Of.Grounds.size(); ++K)
		{
			const std::string& Word = Of.Grounds[K];
			Words[K] = Word == "woods" ? Paint::Woods : Word == "water" ? Paint::Water
				: Word == "buildings" || Word == "industry" || Word == "suburb" ? Paint::Building : Paint::Ground;
			Crossing[K] = Word == "bridge";
		}
		const bool bStoreys = Of.Storeys.size() >= Cells;
		for (int Row = 0; Row < Out.Rows; ++Row)
		{
			Run Open;
			for (int Col = 0; Col <= Out.Cols; ++Col)
			{
				Paint What = Paint::Ground;
				if (Col < Out.Cols)
				{
					// (the square's box on the field: its two opposite corners, whichever way the map is turned)
					double X0 = 0.0, Y0 = 0.0, X1 = 0.0, Y1 = 0.0;
					In.ToField(In.X + Col * Square, In.Y + Row * Square, X0, Y0);
					In.ToField(std::min(In.X + (Col + 1) * Square, In.X + In.W), std::min(In.Y + (Row + 1) * Square, In.Y + In.H), X1, Y1);
					const double West = std::min(X0, X1), East = std::max(X0, X1), South = std::min(Y0, Y1), North = std::max(Y0, Y1);
					// (the field's cells it lies on: one whose edge it only touches is not under it)
					const int C0 = std::clamp(static_cast<int>(std::floor(West / Of.Cell + 1e-6)), 0, Of.Cols - 1);
					const int C1 = std::clamp(static_cast<int>(std::ceil(East / Of.Cell - 1e-6)) - 1, C0, Of.Cols - 1);
					const int R0 = std::clamp(static_cast<int>(std::floor(South / Of.Cell + 1e-6)), 0, Of.Rows - 1);
					const int R1 = std::clamp(static_cast<int>(std::ceil(North / Of.Cell - 1e-6)) - 1, R0, Of.Rows - 1);
					int Under = 0, Wooded = 0;
					bool bWater = false, bBuilt = false, bBridge = false;
					for (int R = R0; R <= R1; ++R)
					{
						for (int C = C0; C <= C1; ++C)
						{
							const std::size_t At = static_cast<std::size_t>(R) * static_cast<std::size_t>(Of.Cols) + static_cast<std::size_t>(C);
							const std::size_t Kind = Of.Ground[At];
							const Paint Here = Kind < Words.size() ? Words[Kind] : Paint::Ground;
							++Under;
							Wooded += Here == Paint::Woods ? 1 : 0;
							bWater = bWater || Here == Paint::Water;
							bBridge = bBridge || (Kind < Crossing.size() && Crossing[Kind]);
							bBuilt = bBuilt || Here == Paint::Building || (bStoreys && Of.Storeys[At] > 0 && Here != Paint::Water);
						}
					}
					// (a bridge wins over all, water too: its square is plain ground, so each crossing shows as a gap in the river)
					What = bBridge ? Paint::Ground : bWater ? Paint::Water : bBuilt ? Paint::Building
						: Wooded * 2 >= Under && Wooded > 0 ? Paint::Woods : Paint::Ground;
				}
				// (a run ends where the paint changes, and at the row's end)
				if (Open.Count > 0 && (Col == Out.Cols || What != Open.What))
				{
					Out.Runs.push_back(Open);
					Open = Run();
				}
				if (Col < Out.Cols && What != Paint::Ground)
				{
					if (Open.Count == 0)
					{
						Open.Row = Row;
						Open.Col = Col;
						Open.What = What;
					}
					++Open.Count;
				}
			}
		}
		return Out;
	}

	void RunBox(const Painted& Of, const Frame& In, const Run& One, double& OutX, double& OutY, double& OutW, double& OutH)
	{
		OutX = In.X + One.Col * Of.Square;
		OutY = In.Y + One.Row * Of.Square;
		OutW = std::max(0.0, std::min(OutX + One.Count * Of.Square, In.X + In.W) - OutX);
		OutH = std::max(0.0, std::min(OutY + Of.Square, In.Y + In.H) - OutY);
	}

	void Mark(const Frame& In, double FieldX, double FieldY, double Wide, double Deep, double Face,
		double& OutAx, double& OutAy, double& OutBx, double& OutBy, double& OutThick)
	{
		double Cx = 0.0, Cy = 0.0;
		In.ToMap(FieldX, FieldY, Cx, Cy);
		// (it faces (sin, cos) on the field, north being y; its frontage runs square to that, (cos, -sin). On the map
		// north is up the screen, so y turns over; turned half round, both turn back)
		const double Turn = In.bTurned ? -1.0 : 1.0;
		const double Fx = std::cos(Face) * Turn, Fy = std::sin(Face) * Turn;
		const double Half = std::max(MarkLong, Wide * In.Scale()) * 0.5;
		OutAx = Cx - Fx * Half;
		OutAy = Cy - Fy * Half;
		OutBx = Cx + Fx * Half;
		OutBy = Cy + Fy * Half;
		OutThick = std::max(MarkDeep, Deep * In.Scale());
	}

	std::vector<std::pair<double, double>> Outline(const Frame& In, const std::vector<std::pair<double, double>>& FieldCorners)
	{
		std::vector<std::pair<double, double>> Shape;
		if (!In.Valid() || FieldCorners.size() < 3)
		{
			return Shape;
		}
		for (const std::pair<double, double>& Corner : FieldCorners)
		{
			double Mx = 0.0, My = 0.0;
			In.ToMap(Corner.first, Corner.second, Mx, My);
			Shape.push_back({ Mx, My });
		}
		// Cut to the map's box, an edge at a time (each edge keeps what is on its inner side: a point's Keep is how far
		// inside it is, and a side that crosses the edge is cut where Keep is nothing).
		for (int Edge = 0; Edge < 4 && !Shape.empty(); ++Edge)
		{
			auto Keep = [&In, Edge](const std::pair<double, double>& P)
			{
				return Edge == 0 ? P.first - In.X : Edge == 1 ? In.X + In.W - P.first : Edge == 2 ? P.second - In.Y : In.Y + In.H - P.second;
			};
			std::vector<std::pair<double, double>> Cut;
			for (std::size_t K = 0; K < Shape.size(); ++K)
			{
				const std::pair<double, double>& Here = Shape[K];
				const std::pair<double, double>& Next = Shape[(K + 1) % Shape.size()];
				const double KeepHere = Keep(Here), KeepNext = Keep(Next);
				if (KeepHere >= 0.0)
				{
					Cut.push_back(Here);
				}
				if ((KeepHere >= 0.0) != (KeepNext >= 0.0))
				{
					const double Along = KeepHere / (KeepHere - KeepNext);
					Cut.push_back({ Here.first + (Next.first - Here.first) * Along, Here.second + (Next.second - Here.second) * Along });
				}
			}
			Shape = Cut;
		}
		if (Shape.size() < 3)
		{
			Shape.clear();
			return Shape;
		}
		Shape.push_back(Shape.front());
		return Shape;
	}
}
