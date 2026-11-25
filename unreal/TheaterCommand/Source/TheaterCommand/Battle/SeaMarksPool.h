#pragma once

// DrawMarks' meshes kept (the release 2 ocean plan, step 1(a): pay first). Today ASeaBattle::DrawMarks lays the sea's marks
// afresh every frame (the wakes' churned bands and their foam, each ship's bow wave, the white water and the foam wall along
// her sides, the arms of her wave and the boil at her stern, the torpedo tracks, the rings round men in the water, a broken
// ship's boil and the oil) and makes both its meshes again with CreateMeshSection, so the engine makes their render proxies
// afresh every frame, and clears a mesh with nothing to draw every frame it has nothing (ClearMeshSection: a proxy made
// afresh each time too). Under -TheaterSeaMarksPool=1 each mesh keeps its shape: the frame's own points and triangles, as
// DrawMarks lays them (its ribbons' and rings' points shared, as today), are held to the shape the mesh was last made with,
// and while the shape holds (the same number of points, the same triangles in the same order) only the points are sent again
// in place (UpdateMeshSection, which a plain UProceduralMeshComponent may take and a UTheaterStillMesh never: their places,
// UVs and colours; the normals as made, DrawMarks laying every one straight up). A frame whose shape changed makes the mesh
// afresh, as today; a frame with nothing to draw clears it once, then sends nothing until there is something again. So no
// frame sends more points than today's, and what is drawn is today's, triangle for triangle: no capacity, no growing, no
// region per kind of mark (regions, or a mesh cut into several sections, would draw the see-through foam in another order).
// A pool of fixed quads was built first and dropped: four points a quad, none shared, it sent four to five times today's
// points every frame (the review of 24 November). Engine-free: Tools/sea_waves/marks_pool_test.cpp builds it in the cloud
// and plays DrawMarks' own source through it, today's way and kept, frame by frame over every recorded action and stand-in.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace SeaMarksPool
{
	/** What a frame sends a mesh: nothing (nothing to draw, and nothing held), the mesh cleared (nothing to draw this frame,
	 * something held: ClearMeshSection, once), the mesh made afresh (its shape new or changed: CreateMeshSection, as today),
	 * or its points sent again in place (the shape held: UpdateMeshSection). */
	enum class Send
	{
		Nothing,
		Clear,
		Make,
		Update
	};

	/** One mesh's shape, as last made (the foam's on the wake ink, or the marks': the oil, and the foam where there is no
	 * wake ink), and the battle's tally of what it was sent. */
	struct Shape
	{
		/** Whether the mesh holds a section made from Points and Tris (made, and not cleared since). */
		bool bHeld = false;
		int Points = 0;
		std::vector<int32_t> Tris;
		/** What this frame sent. */
		Send This = Send::Nothing;
		/** The battle's tally: frames, the frames the mesh was made afresh, sent again in place, cleared, and sent nothing;
		 * the points sent in all (made or sent again), and the most in one frame. */
		int Frames = 0, Made = 0, Updated = 0, Cleared = 0, Idle = 0;
		long long PointsSent = 0;
		int MostPoints = 0;
	};

	/**
	 * One frame of a mesh: Tris (TriIndices of them, three a triangle) over Points points, as DrawMarks would hand
	 * CreateMeshSection. Returns, and keeps in This, what the mesh is sent: made afresh when it holds nothing or its shape
	 * changed, sent again in place while the same points and triangles hold, cleared once when there is nothing to draw.
	 */
	inline Send Next(Shape& S, const int32_t* Tris, int TriIndices, int Points)
	{
		++S.Frames;
		if (Points <= 0)
		{
			if (S.bHeld)
			{
				S.bHeld = false;
				S.Points = 0;
				S.Tris.clear();
				++S.Cleared;
				return S.This = Send::Clear;
			}
			++S.Idle;
			return S.This = Send::Nothing;
		}
		S.PointsSent += Points;
		S.MostPoints = Points > S.MostPoints ? Points : S.MostPoints;
		const int Count = TriIndices > 0 ? TriIndices : 0;
		if (S.bHeld && Points == S.Points && Count == static_cast<int>(S.Tris.size()) && (Count == 0 || std::memcmp(Tris, S.Tris.data(), sizeof(int32_t) * static_cast<size_t>(Count)) == 0))
		{
			++S.Updated;
			return S.This = Send::Update;
		}
		S.bHeld = true;
		S.Points = Points;
		S.Tris.assign(Tris, Tris + Count);
		++S.Made;
		return S.This = Send::Make;
	}

	/** "foam's", "marks'": whose mesh. */
	inline std::string Whose(const char* Which)
	{
		const std::string Name = Which;
		return Name + (!Name.empty() && Name.back() == 's' ? "'" : "'s");
	}

	/** The meshes' line, once a battle (with the threads' and the "Sea look:" line, under -TheaterSeaMarksPool=1). */
	inline std::string Line(const Shape& Foam, const Shape& Marks)
	{
		auto One = [](const char* Which, const Shape& S)
		{
			char Text[320];
			std::snprintf(Text, sizeof(Text),
				"the %s mesh sent again in place %d of %d frames, made afresh %d, cleared %d, sent nothing %d (%.0f points a frame on the mean, at most %d: "
				"today's own)",
				Whose(Which).c_str(), S.Updated, S.Frames, S.Made, S.Cleared, S.Idle, S.Frames > 0 ? static_cast<double>(S.PointsSent) / S.Frames : 0.0, S.MostPoints);
			return std::string(Text);
		};
		return "Sea marks pool: " + One("foam", Foam) + "; " + One("marks", Marks) + ".";
	}
}
