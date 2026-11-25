// The release 2 ocean plan, step 1(a) (pay first): DrawMarks' meshes kept, -TheaterSeaMarksPool=1. The cloud test plays the
// game's own ASeaBattle::DrawMarks (and LayWakes, and step 1(a)'s DrawMarksPooled and PoolMarks), cut out of
// Battle/SeaBattle.cpp as it stands by marks_extract.py and built against the engine's stand-ins (marks_shim.h), over every
// recorded action and stand-in, frame by frame at the scene's 30 frames a second: four scenes on one show, today's way and
// kept, each with the wake ink (the foam on its own mesh, its churned bands under it) and without (the foam on the marks'
// mesh, its standing sheets drawn both ways round). Every frame the kept meshes must draw what today's draw: the same
// triangles in the same order, each the same three points (place, normal, UV, colour), the same bounds, the same material.
// And each mesh must keep to step 1(a)'s rule: made afresh (as today) exactly when its shape is new or changed, its points
// sent again in place exactly while the same points and triangles hold, cleared once when there is nothing to draw, then
// sent nothing; never sending more points in a frame than today's, and never making its render proxy afresh more often.
// What it saves is counted (the frames each mesh's proxy is kept), and what the engine's own data path costs either way is
// emulated beside it (the per-point work of CreateMeshSection and the proxy made from it, against UpdateMeshSection's and
// the render thread's: the Mac's A/B times the rest). The kinds of mark DrawMarks lays are found in today's own meshes,
// each by the shape and paint the game gives it. Before the recordings, the rule's own cases (Battle/SeaMarksPool.h), and
// the game's lines this stands beside, pinned.
// Usage: marks_pool_test SRC recording.json ...   (SRC: Source/TheaterCommand; built by Tools/sea_waves/run.sh, which
// first runs marks_extract.py into the folder it builds in)

#include "marks_shim.h"

#include "Battle/SeaFeed.h"
#include "Battle/SeaMarksPool.h"
#include "Battle/SeaShow.h"

#include <array>
#include <chrono>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>

namespace SeaParts
{
#include "marks_parts.inc"
}

/** The scene's state, as much of it as DrawMarks and LayWakes read (FSeaBattleState: its show and its clock). */
struct FSeaBattleState
{
	explicit FSeaBattleState(uint64 Seed) : Show(Seed) {}
	SeaShow::Show Show;
	double BattleT = 0.0;
};

/** ASeaBattle, as much of it as DrawMarks and LayWakes use: its own structs cut out of SeaBattle.h, and its members
 * declared word for word as SeaBattle.h declares them (each line pinned below). */
class ASeaBattle
{
public:
#include "marks_structs.inc"

	TSharedPtr<FSeaBattleState> State;
	TObjectPtr<UProceduralMeshComponent> Marks;
	TObjectPtr<UProceduralMeshComponent> Foam;
	TObjectPtr<UMaterialInstanceDynamic> MarkInk;
	TObjectPtr<UMaterialInstanceDynamic> FoamInk;
	double UnitsPerMetre = 100.0;
	TMap<int32, double> DrawnLong;
	TMap<int32, FBreakUp> Breaks;
	TMap<int32, TArray<FWakePoint>> Wakes;
	TMap<int32, FTrackLine> Tracks;
	TSet<int32> MenJumping;
	TMap<int32, double> MenLanded;
	double WakeBright = 0.85, WakeStrength = 1.0;
	FVector EyeAt = FVector::ZeroVector;
	bool bMarksPool = false;
	SeaMarksPool::Shape FoamShape, MarksShape;

	void LayWakes();
	void DrawMarks();
	void DrawMarksPooled(const TArray<FVector>& FV, const TArray<FVector>& FN, const TArray<FVector2D>& FU, const TArray<FColor>& FC, const TArray<int32>& FT,
		TArray<FVector>& DV, TArray<FVector>& DN, TArray<FColor>& DC, TArray<int32>& DT, bool bInk);
	void PoolMarks(SeaMarksPool::Shape& Into, UProceduralMeshComponent* Mesh, const TArray<FVector>& V, const TArray<int32>& T, const TArray<FVector>& N, const TArray<FVector2D>& U,
		const TArray<FColor>& C, UMaterialInterface* Ink);
	FVector World(double X, double Y, double Z) const;
	FVector BoardOf(const FVector& WorldAt) const;
};

#include "marks_bodies.inc"

namespace
{
	int Checks = 0, Failed = 0;

	void Check(const std::string& What, bool bOk, const std::string& Got)
	{
		++Checks;
		Failed += bOk ? 0 : 1;
		std::printf("  %s  %s: %s\n", bOk ? "ok  " : "FAIL", What.c_str(), Got.c_str());
	}

	void Note(const std::string& What, const std::string& Got) { std::printf("  note  %s: %s\n", What.c_str(), Got.c_str()); }

	std::string Num(double X, int Places = 1)
	{
		char Text[64];
		std::snprintf(Text, sizeof(Text), "%.*f", Places, X);
		return Text;
	}

	std::string Share(long long Part, long long Whole) { return Num(100.0 * static_cast<double>(Part) / static_cast<double>(std::max<long long>(1, Whole)), 1) + "%"; }

	std::string ReadAll(const std::string& Path)
	{
		std::ifstream File(Path, std::ios::binary);
		std::stringstream Text;
		Text << File.rdbuf();
		return Text.str();
	}

	bool Has(const std::string& Text, const std::string& Part) { return Text.find(Part) != std::string::npos; }

	int CountOf(const std::string& Text, const std::string& Part)
	{
		int N = 0;
		for (size_t K = Text.find(Part); K != std::string::npos; K = Text.find(Part, K + 1)) ++N;
		return N;
	}

	// ---------------------------------------------------------------------------------------------------------------
	// The game's own lines this stands beside: the members it declares as SeaBattle.h does, and the scene's own feeding
	// of what DrawMarks draws from (the torpedo tracks, PlayEffects; a ship broken in two, RunTrails), mirrored below.

	struct Pin
	{
		const char* File;
		const char* Line;
	};
	const Pin Pins[] = {
		{ "Battle/SeaBattle.h", "TSharedPtr<FSeaBattleState> State;" },
		{ "Battle/SeaBattle.h", "TObjectPtr<UProceduralMeshComponent> Marks;" },
		{ "Battle/SeaBattle.h", "TObjectPtr<UProceduralMeshComponent> Foam;" },
		{ "Battle/SeaBattle.h", "TObjectPtr<UMaterialInstanceDynamic> MarkInk;" },
		{ "Battle/SeaBattle.h", "TObjectPtr<UMaterialInstanceDynamic> FoamInk;" },
		{ "Battle/SeaBattle.h", "double UnitsPerMetre = 100.0;" },
		{ "Battle/SeaBattle.h", "TMap<int32, double> DrawnLong;" },
		{ "Battle/SeaBattle.h", "TMap<int32, FBreakUp> Breaks;" },
		{ "Battle/SeaBattle.h", "TMap<int32, TArray<FWakePoint>> Wakes;" },
		{ "Battle/SeaBattle.h", "TMap<int32, FTrackLine> Tracks;" },
		{ "Battle/SeaBattle.h", "TSet<int32> MenJumping;" },
		{ "Battle/SeaBattle.h", "TMap<int32, double> MenLanded;" },
		{ "Battle/SeaBattle.h", "double WakeBright = 0.85, WakeStrength = 1.0;" },
		{ "Battle/SeaBattle.h", "FVector EyeAt = FVector::ZeroVector;" },
		{ "Battle/SeaBattle.h", "bool bMarksPool = false;" },
		{ "Battle/SeaBattle.h", "SeaMarksPool::Shape FoamShape, MarksShape;" },
		{ "Battle/SeaBattle.h", "#include \"Battle/SeaMarksPool.h\"" },
		{ "Battle/SeaBattle.h", "void LayWakes();\n\tvoid DrawMarks();" },
		{ "Battle/SeaBattle.h", "\tvoid DrawMarksPooled(const TArray<FVector>& FV, const TArray<FVector>& FN, const TArray<FVector2D>& FU, const TArray<FColor>& FC, const TArray<int32>& FT,\n"
								"\t\tTArray<FVector>& DV, TArray<FVector>& DN, TArray<FColor>& DC, TArray<int32>& DT, bool bInk);" },
		{ "Battle/SeaBattle.h", "\tvoid PoolMarks(SeaMarksPool::Shape& Into, UProceduralMeshComponent* Mesh, const TArray<FVector>& V, const TArray<int32>& T, const TArray<FVector>& N, const TArray<FVector2D>& U,\n"
								"\t\tconst TArray<FColor>& C, UMaterialInterface* Ink);" },
		{ "Battle/SeaBattle.h", "FVector World(double X, double Y, double Z) const;" },
		{ "Battle/SeaBattle.h", "FVector BoardOf(const FVector& WorldAt) const;" },
		// (the scene's state: its show and clock)
		{ "Battle/SeaBattle.cpp", "\tSeaShow::Show Show;\n\tsize_t Taken = 0;       // the recording's views handed to the show so far\n\tdouble BattleT = 0.0;" },
		// (the frame's order: the show stepped, its wakes laid and its effects played, then the breaks, then the marks)
		{ "Battle/SeaBattle.cpp", "\tS.Show.Eye = SeaShow::Vec(Seen.X, Seen.Y, FMath::Max(Seen.Z, 2.0));\n\tS.Show.Advance(T);\n\tS.BattleT = T;\n\tLayAndSteer();\n\tLayWakes();\n\tPlayEffects(bSkip);" },
		{ "Battle/SeaBattle.cpp", "\tStage->Advance(S.BattleT);\n\tDrawPlanes();\n\tRunTrails();\n\tDrawMarks();\n\tAimEye();" },
		{ "Battle/SeaBattle.cpp", "S.Show.SetMine(FParse::Param(FCommandLine::Get(), TEXT(\"TheaterSeaShowAll\")) ? -1 : SeaFeed::SideOf(S.Rec.Views.front().Head.Me));" },
		// (the torpedo tracks, PlayEffects)
		{ "Battle/SeaBattle.cpp", "case SeaShow::FxKind::TrackStart:" },
		{ "Battle/SeaBattle.cpp", "FTrackLine& Line = Tracks.FindOrAdd(E.Key);\n\t\t\tLine.From = Line.Head = FVector2D(E.At.X, E.At.Y);\n\t\t\tLine.StopT = -1.0;" },
		{ "Battle/SeaBattle.cpp", "Line.bOxygen = (KitNation(E.Text) == TEXT(\"jp\") && S.Rec.Board.Year >= 1938) || E.What == \"wakeless\";" },
		{ "Battle/SeaBattle.cpp", "case SeaShow::FxKind::TrackMove:\n\t\t\tif (FTrackLine* Line = Tracks.Find(E.Key))\n\t\t\t{\n\t\t\t\tLine->Head = FVector2D(E.At.X, E.At.Y);\n\t\t\t}" },
		{ "Battle/SeaBattle.cpp", "case SeaShow::FxKind::TrackStop:\n\t\t\tif (FTrackLine* Line = Tracks.Find(E.Key))\n\t\t\t{\n\t\t\t\tLine->Head = FVector2D(E.At.X, E.At.Y);\n\t\t\t\tLine->StopT = Now;\n\t\t\t}" },
		// (a ship broken in two, RunTrails: the stage's cut, which the stage alone knows, stood in for by her place as she goes)
		{ "Battle/SeaBattle.cpp", "if (Ship.Doing != SeaShow::Fate::Sinking || Breaks.Contains(Ship.Key) || !Stage->BreakOf(Ship.Key, Cut, When))" },
		{ "Battle/SeaBattle.cpp", "FBreakUp& Made = Breaks.Add(Ship.Key);" },
		{ "Battle/SeaBattle.cpp", "Made.T = FMath::Min(When, Now);\n\t\t\tMade.Heading = Ship.Heading;\n\t\t\tMade.Long = DrawnLong.Contains(Ship.Key) ? DrawnLong[Ship.Key] : Ship.Long;\n\t\t\tMade.Beam = Ship.Beam;" },
		// (step 1(a)'s switch, read once as the scene opens, and its meshes' line once a battle)
		{ "Battle/SeaBattle.cpp", "bMarksPool = SeaLookSwitches::TakeOnOff(SeaLook, \"TheaterSeaMarksPool=\");" },
		{ "Battle/SeaBattle.cpp", "const FString Pools(UTF8_TO_TCHAR(SeaMarksPool::Line(FoamShape, MarksShape).c_str()));" },
		{ "Battle/SeaLookSwitches.h", "{ \"TheaterSeaMarksPool=\", \"0\", \"own\", \"1\", true }," },
	};

	/** The text of a function of the game's, from its first line to the next function's (for the checks on its words). */
	std::string Body(const std::string& Text, const std::string& Head)
	{
		const size_t At = Text.find(Head);
		if (At == std::string::npos) return std::string();
		const size_t End = Text.find("\n}\n", At);
		return Text.substr(At, End == std::string::npos ? std::string::npos : End + 3 - At);
	}

	// ---------------------------------------------------------------------------------------------------------------
	// What a mesh draws: its triangles in order, each its three points whole (place, normal, tangent, colour, UV).

	struct Tri
	{
		const FProcMeshVertex* P[3];
	};

	void TrianglesOf(const UProceduralMeshComponent& Mesh, std::vector<Tri>& Out)
	{
		Out.clear();
		if (Mesh.ProcMeshSections.empty() || !Mesh.ProcMeshSections[0].bSectionVisible) return;
		const FProcMeshSection& S = Mesh.ProcMeshSections[0];
		for (size_t K = 0; K + 2 < S.ProcIndexBuffer.size(); K += 3)
		{
			Tri T;
			for (int J = 0; J < 3; ++J) T.P[J] = &S.ProcVertexBuffer[S.ProcIndexBuffer[K + static_cast<size_t>(J)]];
			Out.push_back(T);
		}
	}

	/** A triangle with two of its points in one place: no area, nothing drawn. */
	bool NoArea(const Tri& T) { return T.P[0]->Position == T.P[1]->Position || T.P[1]->Position == T.P[2]->Position || T.P[0]->Position == T.P[2]->Position; }

	struct Same
	{
		bool bSame = true;
		std::string Why;
		int Triangles = 0, Flat = 0;
	};

	/** Whether Kept draws what Today draws: today's every triangle in order, each the same three points (place, normal,
	 * tangent, colour, UV) in the same order, and nothing else (a triangle of no area, which the engine keeps and the chip
	 * draws nothing of, held to as any other); the same ink (each scene's own: 1 the wake ink, 2 the marks' ink, 0 none);
	 * the same bounds wherever today's has points. */
	Same Compare(const UProceduralMeshComponent& Today, const UProceduralMeshComponent& Kept, int TodayInk, int KeptInk, std::vector<Tri>& A, std::vector<Tri>& B)
	{
		Same Out;
		TrianglesOf(Today, A);
		TrianglesOf(Kept, B);
		if (A.size() != B.size())
		{
			Out.bSame = false;
			Out.Why = "kept draws " + std::to_string(B.size()) + " triangles, today's " + std::to_string(A.size());
			return Out;
		}
		for (size_t J = 0; J < A.size(); ++J)
		{
			for (int K = 0; K < 3; ++K)
			{
				if (!(*A[J].P[K] == *B[J].P[K]))
				{
					Out.bSame = false;
					Out.Why = "triangle " + std::to_string(J) + " of " + std::to_string(A.size()) + ", its point " + std::to_string(K) + ", is not today's";
					return Out;
				}
			}
			Out.Flat += NoArea(A[J]) ? 1 : 0;
		}
		Out.Triangles = static_cast<int>(A.size());
		const bool bTodayHas = !Today.ProcMeshSections.empty() && !Today.ProcMeshSections[0].ProcVertexBuffer.empty();
		if (bTodayHas && Kept.ProcMeshSections.empty())
		{
			Out.bSame = false;
			Out.Why = "today's has points, kept holds no section";
			return Out;
		}
		if (bTodayHas && !(Today.ProcMeshSections[0].SectionLocalBox == Kept.ProcMeshSections[0].SectionLocalBox))
		{
			Out.bSame = false;
			Out.Why = "the bounds differ";
			return Out;
		}
		if (bTodayHas && TodayInk != KeptInk)
		{
			Out.bSame = false;
			Out.Why = "the material differs";
		}
		return Out;
	}

	int PointsOf(const UProceduralMeshComponent& Mesh) { return Mesh.ProcMeshSections.empty() ? 0 : static_cast<int>(Mesh.ProcMeshSections[0].ProcVertexBuffer.size()); }

	// ---------------------------------------------------------------------------------------------------------------
	// The engine's own data path, emulated (ProceduralMeshComponent.cpp as UE 5 has it): the work done for each point and
	// index, not the scene's own (the render proxy's removal and adding, the primitive's scene info), which only the Mac
	// can time. Made afresh: the section built (FProcMeshVertex, 152 bytes a point; the indices clamped), then at the
	// frame's end the proxy made from it (each point turned into FDynamicMeshVertex, the four vertex buffers and the index
	// buffer filled, and each uploaded into new memory). Sent again in place: the arrays given written into the section,
	// the whole section copied into the render command (FProcMeshSectionUpdateData), and on the render thread each point
	// turned and written, the four buffers locked and copied into the proxy's own memory.

	namespace Engine
	{
		struct V3d
		{
			double X, Y, Z;
		};
		struct V2d
		{
			double X, Y;
		};
		struct V3f
		{
			float X, Y, Z;
		};
		struct V2f
		{
			float X, Y;
		};
		struct Tangent
		{
			V3d X;
			bool bFlip;
		};
		struct ProcVertex // (FProcMeshVertex: Position, Normal, Tangent, Color, UV0-UV3)
		{
			V3d Position, Normal;
			Tangent T;
			FColor Color;
			V2d UV0, UV1, UV2, UV3;
		};
		struct DynVertex // (FDynamicMeshVertex: Position, TextureCoordinate[MAX_STATIC_TEXCOORDS], TangentX, TangentZ, Color)
		{
			V3f Position;
			V2f Tex[8];
			uint32 TangentX, TangentZ;
			FColor Color;
		};
		/** A proxy's buffers: positions, tangents (two packed a point), four UVs a point, colours; indices; and the "chip's"
		 * memory they are copied into. */
		struct Proxy
		{
			std::vector<V3f> Pos;
			std::vector<uint32> Tangents;
			std::vector<V2f> Uvs;
			std::vector<FColor> Colors;
			std::vector<uint32> Indices;
			std::vector<char> Chip;
		};
		struct Section
		{
			std::vector<ProcVertex> Points;
			std::vector<uint32> Indices;
			V3d Lo{}, Hi{};
		};

		uint32 Pack(const V3d& N)
		{
			auto Q = [](double V) { return static_cast<uint32>(static_cast<uint8>(std::lround(std::clamp(V, -1.0, 1.0) * 127.5 + 127.5))); };
			return Q(N.X) | Q(N.Y) << 8 | Q(N.Z) << 16 | 127u << 24;
		}
		void Turn(DynVertex& D, const ProcVertex& P)
		{
			D.Position = V3f{ static_cast<float>(P.Position.X), static_cast<float>(P.Position.Y), static_cast<float>(P.Position.Z) };
			D.Color = P.Color;
			D.TangentX = Pack(P.T.X);
			D.TangentZ = Pack(P.Normal);
			const V2d* Uv[4] = { &P.UV0, &P.UV1, &P.UV2, &P.UV3 };
			for (int K = 0; K < 4; ++K) D.Tex[K] = V2f{ static_cast<float>(Uv[K]->X), static_cast<float>(Uv[K]->Y) };
		}
		void Box(Section& S, const V3d& P, bool bFirst)
		{
			if (bFirst)
			{
				S.Lo = S.Hi = P;
				return;
			}
			S.Lo = V3d{ std::min(S.Lo.X, P.X), std::min(S.Lo.Y, P.Y), std::min(S.Lo.Z, P.Z) };
			S.Hi = V3d{ std::max(S.Hi.X, P.X), std::max(S.Hi.Y, P.Y), std::max(S.Hi.Z, P.Z) };
		}
		void Fill(Proxy& P, size_t K, const DynVertex& D)
		{
			P.Pos[K] = D.Position;
			P.Tangents[2 * K] = D.TangentX;
			P.Tangents[2 * K + 1] = D.TangentZ;
			for (size_t T = 0; T < 4; ++T) P.Uvs[4 * K + T] = D.Tex[T];
			P.Colors[K] = D.Color;
		}
		void Upload(Proxy& P, bool bIndices)
		{
			const size_t N = P.Pos.size();
			char* At = P.Chip.data();
			std::memcpy(At, P.Pos.data(), N * sizeof(V3f));
			At += N * sizeof(V3f);
			std::memcpy(At, P.Tangents.data(), N * 2 * sizeof(uint32));
			At += N * 2 * sizeof(uint32);
			std::memcpy(At, P.Uvs.data(), N * 4 * sizeof(V2f));
			At += N * 4 * sizeof(V2f);
			std::memcpy(At, P.Colors.data(), N * sizeof(FColor));
			At += N * sizeof(FColor);
			if (bIndices) std::memcpy(At, P.Indices.data(), P.Indices.size() * sizeof(uint32));
		}
		size_t ChipBytes(size_t Points, size_t Indices) { return Points * (sizeof(V3f) + 2 * sizeof(uint32) + 4 * sizeof(V2f) + sizeof(FColor)) + Indices * sizeof(uint32); }

		/** The frame's arrays, as DrawMarks hands them over (taken from today's own section, not timed). */
		struct Frame
		{
			std::vector<V3d> V, N;
			std::vector<V2d> U;
			std::vector<FColor> C;
			std::vector<int32> T;
		};

		volatile double Sink = 0.0;

		/** CreateMeshSection and the proxy made afresh from it. */
		void Make(Section& S, Proxy& P, const Frame& F)
		{
			const size_t N = F.V.size();
			S.Points.resize(N);
			for (size_t K = 0; K < N; ++K)
			{
				ProcVertex& V = S.Points[K];
				V.Position = F.V[K];
				V.Normal = F.N.size() == N ? F.N[K] : V3d{ 0.0, 0.0, 1.0 };
				V.UV0 = F.U.size() == N ? F.U[K] : V2d{ 0.0, 0.0 };
				V.UV1 = V.UV2 = V.UV3 = V2d{ 0.0, 0.0 };
				V.Color = F.C.size() == N ? F.C[K] : FColor(255, 255, 255);
				V.T = Tangent{ V3d{ 1.0, 0.0, 0.0 }, false };
				Box(S, V.Position, K == 0);
			}
			const int32 Last = static_cast<int32>(N) - 1;
			S.Indices.resize((F.T.size() / 3) * 3);
			for (size_t K = 0; K < S.Indices.size(); ++K) S.Indices[K] = static_cast<uint32>(std::min(F.T[K], Last));
			// (the proxy: its own buffers, new)
			Proxy Fresh;
			std::vector<DynVertex> Turned(N);
			for (size_t K = 0; K < N; ++K) Turn(Turned[K], S.Points[K]);
			Fresh.Pos.resize(N);
			Fresh.Tangents.resize(2 * N);
			Fresh.Uvs.resize(4 * N);
			Fresh.Colors.resize(N);
			for (size_t K = 0; K < N; ++K) Fill(Fresh, K, Turned[K]);
			Fresh.Indices = S.Indices;
			Fresh.Chip.resize(ChipBytes(N, Fresh.Indices.size()));
			Upload(Fresh, true);
			P = std::move(Fresh);
			Sink = Sink + (N ? P.Chip[N % P.Chip.size()] : 0) + S.Hi.X;
		}

		/** UpdateMeshSection (the places, UVs where given and colours; no normals) and the render thread's update. */
		void Update(Section& S, Proxy& P, const Frame& F)
		{
			const size_t N = F.V.size();
			for (size_t K = 0; K < N; ++K)
			{
				ProcVertex& V = S.Points[K];
				V.Position = F.V[K];
				Box(S, V.Position, K == 0);
				if (F.U.size() == N) V.UV0 = F.U[K];
				if (F.C.size() == N) V.Color = F.C[K];
			}
			std::vector<ProcVertex> Sent(S.Points); // (SectionData->NewVertexBuffer = Section.ProcVertexBuffer)
			for (size_t K = 0; K < N; ++K)
			{
				DynVertex D;
				Turn(D, Sent[K]);
				Fill(P, K, D);
			}
			Upload(P, false);
			Sink = Sink + (N ? P.Chip[N % P.Chip.size()] : 0) + S.Hi.X;
		}

		/** The frame's arrays from a section of today's (its points, their normals, UVs and colours, and its indices). */
		void FrameOf(const UProceduralMeshComponent& Mesh, bool bUvs, Frame& F)
		{
			F = Frame();
			if (Mesh.ProcMeshSections.empty()) return;
			const FProcMeshSection& S = Mesh.ProcMeshSections[0];
			for (const FProcMeshVertex& P : S.ProcVertexBuffer)
			{
				F.V.push_back(V3d{ P.Position.X, P.Position.Y, P.Position.Z });
				F.N.push_back(V3d{ P.Normal.X, P.Normal.Y, P.Normal.Z });
				if (bUvs) F.U.push_back(V2d{ P.UV0.X, P.UV0.Y });
				F.C.push_back(P.Color);
			}
			for (const uint32 K : S.ProcIndexBuffer) F.T.push_back(static_cast<int32>(K));
		}
	}

	// ---------------------------------------------------------------------------------------------------------------
	// The kinds of mark, found in today's own meshes (the scene with the wake ink): each piece of a mesh (a run of
	// triangles over points of its own) by its shape, the points across a row or round a ring and the rows, and by the
	// paint the game gives it (the wake ink's red how white, green how whole, blue 255 for the churned band; the oil's own
	// colours).

	enum EKind
	{
		Bands,
		Wakes,
		BowWaves,
		Sides,
		FoamWalls,
		Arms,
		Boils,
		Tracks,
		MenRings,
		BrokenShips,
		Oil,
		Unknown,
		KindCount
	};
	const char* const KindNames[KindCount] = { "the wakes' churned bands", "the wakes' foam", "bow waves", "the white water along her sides", "foam walls", "the arms of her wave",
		"the boils at her stern", "torpedo tracks", "men's rings", "broken ships' boils and rings", "oil", "not known" };

	struct Piece
	{
		int First = 0, Points = 0, Triangles = 0;
		int Across = 0, Rows = 0; // (0: not a run of like rows or rings)
	};

	/** A triangle's three points, least first (a quad laid either way round, as Quad lays it by which way it faces, has the
	 * same two). */
	std::array<long, 3> Sorted(const std::vector<uint32>& T, size_t At)
	{
		std::array<long, 3> Out = { static_cast<long>(T[At]), static_cast<long>(T[At + 1]), static_cast<long>(T[At + 2]) };
		std::sort(Out.begin(), Out.end());
		return Out;
	}

	/** A mesh's index list cut into pieces: a triangle none of whose points is at or below the piece's highest so far
	 * starts the next (DrawMarks lays each mark's points, then its triangles, one mark after another). Each piece's rows:
	 * the least run of triangles (4, 8, 16, 32 or 64) repeated row after row, each row's triangles the last's points moved
	 * on by one row's (Across), whichever way round each is laid, over Rows rows of Across points; one run alone, two rows,
	 * each half the points. */
	void PiecesOf(const std::vector<uint32>& T, std::vector<Piece>& Out)
	{
		Out.clear();
		size_t Start = 0;
		int Lo = 0, Hi = -1;
		auto Close = [&](size_t End)
		{
			if (End <= Start) return;
			Piece P;
			P.First = Lo;
			P.Points = Hi - Lo + 1;
			P.Triangles = static_cast<int>((End - Start) / 3);
			for (const int Run : { 4, 8, 16, 32, 64 })
			{
				const size_t Ix = End - Start, Step = static_cast<size_t>(Run) * 3;
				if (Ix % Step != 0) continue;
				if (Ix == Step)
				{
					if (P.Points % 2 != 0) continue;
					P.Across = P.Points / 2;
					P.Rows = 2;
					break;
				}
				const long Across = Sorted(T, Start + Step)[0] - Sorted(T, Start)[0];
				if (Across <= 0) continue;
				bool bRows = true;
				for (size_t K = Step; K < Ix && bRows; K += 3)
				{
					const std::array<long, 3> Now = Sorted(T, Start + K), Was = Sorted(T, Start + K - Step);
					bRows = Now[0] == Was[0] + Across && Now[1] == Was[1] + Across && Now[2] == Was[2] + Across;
				}
				const int Rows = static_cast<int>(Ix / Step) + 1;
				if (!bRows || P.Points != Rows * Across) continue;
				P.Across = static_cast<int>(Across);
				P.Rows = Rows;
				break;
			}
			Out.push_back(P);
		};
		for (size_t K = 0; K + 2 < T.size(); K += 3)
		{
			const int Mn = static_cast<int>(std::min({ T[K], T[K + 1], T[K + 2] })), Mx = static_cast<int>(std::max({ T[K], T[K + 1], T[K + 2] }));
			if (Mn > Hi)
			{
				Close(K);
				Start = K;
				Lo = Mn;
				Hi = Mx;
			}
			else
			{
				Lo = std::min(Lo, Mn);
				Hi = std::max(Hi, Mx);
			}
		}
		Close(T.size() - T.size() % 3);
	}

	/** A piece of the foam's mesh on the wake ink, by its shape and paint. Ribbons, five points across: the band's blue
	 * 255; the wake's foam whole in the shares 0.6, 0.94, 1 across; along her side 0.55 and 0.75 (seven rows); the arm
	 * 0.5 throughout (three); the stern's boil 0.85 and 1 (seven); a torpedo's track 0.6 and 1 (thirteen). Sheets three
	 * points across: the bow wave's nine rows, the foam wall's seventeen. Rings: a man's three of 8, a broken ship's three
	 * of 16. (Each as DrawMarks lays it: its Ribbon's WakeAcross, Wholes and stations, its Steps and Round.) */
	EKind FoamKind(const Piece& P, const FProcMeshSection& S)
	{
		auto Pt = [&](int K) -> const FProcMeshVertex& { return S.ProcVertexBuffer[static_cast<size_t>(P.First + K)]; };
		if (P.Across == 5)
		{
			int Best = 0;
			for (int R = 0; R < P.Rows; ++R)
			{
				if (Pt(R * 5 + 2).Color.B == 255) return Bands;
				Best = Pt(R * 5 + 2).Color.G > Pt(Best * 5 + 2).Color.G ? R : Best;
			}
			const int G0 = Pt(Best * 5).Color.G, G1 = Pt(Best * 5 + 1).Color.G, G2 = Pt(Best * 5 + 2).Color.G;
			if (G2 == 0) return Unknown;
			const double Ratio = static_cast<double>(G1) / G2;
			if (G0 < G1) return Wakes;
			if (P.Rows == 13 && Ratio < 0.7) return Tracks;
			if (P.Rows == 7 && Ratio >= 0.65 && Ratio < 0.8) return Sides;
			if (P.Rows == 7 && Ratio >= 0.8 && Ratio < 0.95) return Boils;
			if (P.Rows == 3 && G0 == G1 && G1 == G2) return Arms;
			return Unknown;
		}
		if (P.Across == 3) return P.Rows == 9 ? BowWaves : P.Rows == 17 ? FoamWalls : Unknown;
		if (P.Across == 8 && P.Rows == 3) return MenRings;
		if (P.Across == 16 && P.Rows == 3) return BrokenShips;
		return Unknown;
	}

	/** A piece of the marks' mesh on the wake ink (the oil alone): five rings of 32, its middle in the oil's darkest paint. */
	EKind MarksKind(const Piece& P, const FProcMeshSection& S)
	{
		const FColor& Middle = S.ProcVertexBuffer[static_cast<size_t>(P.First)].Color;
		return P.Across == 32 && P.Rows == 5 && Middle.R == 11 && Middle.G == 10 && Middle.B == 9 ? Oil : Unknown;
	}

	// ---------------------------------------------------------------------------------------------------------------
	// A scene: the game's own DrawMarks with its meshes and inks, today's way or kept, with the wake ink or without.

	struct Scene
	{
		ASeaBattle Game;
		UProceduralMeshComponent FoamMesh, MarksMesh;
		UMaterialInstanceDynamic FoamInkMat, MarkInkMat;
		std::vector<uint32> FoamIndices, MarksIndices; // (a kept mesh's indices as last made)

		Scene(const std::shared_ptr<FSeaBattleState>& InState, bool bInk, bool bPool)
		{
			Game.State = InState;
			Game.Foam = &FoamMesh;
			Game.Marks = &MarksMesh;
			Game.FoamInk = bInk ? &FoamInkMat : nullptr;
			Game.MarkInk = &MarkInkMat;
			Game.bMarksPool = bPool;
		}
		Scene(const Scene&) = delete;
		Scene& operator=(const Scene&) = delete;
	};

	/** The scene's own feeding of the tracks (PlayEffects, its lines pinned above) and the breaks (RunTrails: the stage's
	 * cut stood in for by her place as she starts to go, a ship the show breaks in two), then LayWakes and DrawMarks. */
	void PlayFrame(Scene& Sc, const SeaFeed::Recording& Rec)
	{
		ASeaBattle& G = Sc.Game;
		FSeaBattleState& S = *G.State;
		const double Now = S.BattleT;
		G.LayWakes();
		for (const SeaShow::Fx& E : S.Show.Effects)
		{
			switch (E.Kind)
			{
			case SeaShow::FxKind::TrackStart:
			{
				ASeaBattle::FTrackLine& Line = G.Tracks.FindOrAdd(E.Key);
				Line.From = Line.Head = FVector2D(E.At.X, E.At.Y);
				Line.StopT = -1.0;
				std::string Up;
				for (const char Ch : E.Text) Up += static_cast<char>(std::toupper(static_cast<unsigned char>(Ch)));
				Line.bOxygen = ((Up == "JAP" || Up == "JPN" || Up == "JP") && Rec.Board.Year >= 1938) || E.What == "wakeless";
				break;
			}
			case SeaShow::FxKind::TrackMove:
				if (ASeaBattle::FTrackLine* Line = G.Tracks.Find(E.Key)) Line->Head = FVector2D(E.At.X, E.At.Y);
				break;
			case SeaShow::FxKind::TrackStop:
				if (ASeaBattle::FTrackLine* Line = G.Tracks.Find(E.Key))
				{
					Line->Head = FVector2D(E.At.X, E.At.Y);
					Line->StopT = Now;
				}
				break;
			default:
				break;
			}
		}
		for (const SeaShow::ShipOut& Ship : S.Show.Ships())
		{
			if (Ship.Doing != SeaShow::Fate::Sinking || G.Breaks.Contains(Ship.Key) || Ship.How != "break") continue;
			ASeaBattle::FBreakUp& Made = G.Breaks.Add(Ship.Key);
			Made.At = FVector2D(Ship.At.X, Ship.At.Y);
			Made.T = Now;
			Made.Heading = Ship.Heading;
			Made.Long = G.DrawnLong.Contains(Ship.Key) ? G.DrawnLong[Ship.Key] : Ship.Long;
			Made.Beam = Ship.Beam;
			Made.NextSteam = Made.T;
		}
		Sc.FoamMesh.Tally();
		Sc.MarksMesh.Tally();
		G.DrawMarks();
	}

	/** A kept mesh's frame held to step 1(a)'s rule. bShapeHeld: today's mesh this frame has the points and triangles it
	 * had last frame, which had something to draw, and the kept mesh holds a section (its decision, made independently of
	 * the rule's own). Returns the first rule broken, or empty. */
	std::string ShapeRules(const UProceduralMeshComponent& Mesh, const SeaMarksPool::Shape& S, bool bTodayHas, bool bShapeHeld, bool bHeldBefore, std::vector<uint32>& Made)
	{
		using SeaMarksPool::Send;
		const Send W = S.This;
		if (Mesh.Mismatched != 0) return "sent again with another number of points than it holds";
		if (Mesh.Creates != (W == Send::Make ? 1 : 0)) return std::to_string(Mesh.Creates) + " made this frame";
		if (Mesh.Updates != (W == Send::Update ? 1 : 0)) return std::to_string(Mesh.Updates) + " sent again this frame";
		if (Mesh.Clears != (W == Send::Clear ? 1 : 0)) return std::to_string(Mesh.Clears) + " cleared this frame";
		if (Mesh.MaterialsSet != Mesh.Creates) return "its material set " + std::to_string(Mesh.MaterialsSet) + " times, made " + std::to_string(Mesh.Creates);
		if ((Mesh.Dirtied > 0) != (W == Send::Make || W == Send::Clear)) return "its render proxy made afresh " + std::string(Mesh.Dirtied > 0 ? "though it kept its shape" : "never though it was made or cleared");
		if (!bTodayHas)
		{
			if (W != (bHeldBefore ? Send::Clear : Send::Nothing)) return bHeldBefore ? "nothing to draw, a section held, and not cleared once" : "nothing to draw and nothing held, yet sent";
			return Mesh.ProcMeshSections.empty() || Mesh.ProcMeshSections[0].ProcVertexBuffer.empty() ? std::string() : "nothing to draw, yet a section with points";
		}
		if (W == Send::Update && !bShapeHeld) return "sent again in place though its shape changed";
		if (W == Send::Make && bShapeHeld) return "made afresh though its shape held";
		const FProcMeshSection& Sec = Mesh.ProcMeshSections[0];
		if (W == Send::Make) Made = Sec.ProcIndexBuffer;
		if (Sec.ProcIndexBuffer != Made) return "its triangles changed though only its points were sent";
		if (static_cast<int>(Sec.ProcVertexBuffer.size()) != S.Points) return "holds " + std::to_string(Sec.ProcVertexBuffer.size()) + " points, its shape " + std::to_string(S.Points);
		return std::string();
	}

	/** One mesh's account over a recording, kept against today's. */
	struct Account
	{
		const char* Name = "";
		long long Frames = 0, Differ = 0, Broken = 0, Triangles = 0, NoArea = 0;
		long long TodayPoints = 0, KeptPoints = 0, OverPoints = 0, TodayProxies = 0, KeptProxies = 0;
		long long Made = 0, Updated = 0, Cleared = 0, Idle = 0;
		int MostTodayPoints = 0;
		double TodayEmu = 0.0, KeptEmu = 0.0;
		long long EmuFrames = 0;
		std::string FirstDiffer, FirstBroken, FirstOver;
		// (today's shape last frame, for the rule's decision made independently)
		std::vector<uint32> LastIndices;
		int LastPoints = 0;
		Engine::Section TodaySection, KeptSection;
		Engine::Proxy TodayProxy, KeptProxy;
	};

	/** The eye for the next frame (the scene's AimEye stood in for): beside the first man in the water, so his ring and
	 * those of the men round him are laid; else off the first ship drawn; else up over the board. Board metres. */
	SeaShow::Vec NextEye(const SeaShow::Show& Show)
	{
		using SeaShow::Swim;
		for (const SeaShow::ManOut& Man : Show.Men())
		{
			if (Man.Doing == Swim::Swim || Man.Doing == Swim::Tread || Man.Doing == Swim::Cling) return SeaShow::Vec(Man.At.X + 40.0, Man.At.Y - 60.0, 25.0);
		}
		for (const SeaShow::ShipOut& Ship : Show.Ships())
		{
			if (Ship.bShown && Ship.Doing != SeaShow::Fate::Gone) return SeaShow::Vec(Ship.At.X, Ship.At.Y - 400.0, 60.0);
		}
		return SeaShow::Vec(0.0, -3000.0, 900.0);
	}

	/** A recording's tally, for the sum over them all: each kind's triangles; and for the three meshes drawn (the foam on
	 * the wake ink, the marks with it, the marks without), the points sent and the proxies made, today's and kept. */
	struct Tally
	{
		long long Kinds[KindCount] = {};
		long long TodayPoints[3] = {}, KeptPoints[3] = {}, TodayProxies[3] = {}, KeptProxies[3] = {}, Frames = 0;
	};

	// (every 10th frame's data path emulated, both ways)
	constexpr int EmuEvery = 10;

	void Play(const std::string& Path, Tally& All)
	{
		SeaFeed::Recording Rec;
		std::string Error;
		const std::string Text = ReadAll(Path);
		const bool bRead = SeaFeed::ReadRecording(Text, Rec, Error) && !Rec.Views.empty();
		std::string Name = Path.substr(Path.find_last_of('/') + 1);
		std::printf("%s\n", Name.c_str());
		Check("the recording read", bRead, bRead ? std::to_string(Rec.Views.size()) + " views" : Error);
		if (!bRead) return;
		const auto Began = std::chrono::steady_clock::now();
		auto State = std::make_shared<FSeaBattleState>(Rec.Seed ? Rec.Seed : 239);
		State->Show.SetField(Rec.Board);
		State->Show.SetMine(SeaFeed::SideOf(Rec.Views.front().Head.Me));
		// today's and kept, with the wake ink and without
		Scene TodayInk(State, true, false), KeptInk(State, true, true);
		Scene TodayBare(State, false, false), KeptBare(State, false, true);
		Scene* Scenes[4] = { &TodayInk, &KeptInk, &TodayBare, &KeptBare };
		Account Acc[4];
		Acc[0].Name = "the foam, on the wake ink";
		Acc[1].Name = "the marks (the oil), with the wake ink";
		Acc[2].Name = "the foam's mesh, with no wake ink (never drawn)";
		Acc[3].Name = "the marks (the oil and the foam), with no wake ink";
		long long Kinds[KindCount] = {};
		std::map<std::string, long long> Unknowns;
		std::vector<Tri> A, B;
		std::vector<Piece> Pieces;
		Engine::Frame EmuFrame;
		size_t Taken = 0;
		auto TakeNext = [&]()
		{
			const SeaFeed::Frame& F = Rec.Views[Taken++];
			State->Show.Take(F.Head.T, F.Pieces, F.Events, F.bZones ? &F.Zones : nullptr, F.Head.Contact);
		};
		TakeNext();
		const double BeginT = Rec.Views.front().Head.T, EndT = Rec.Views.back().Head.T;
		const int Frames = static_cast<int>(std::floor((EndT + 12.0 - BeginT) * 30.0)) + 1;
		SeaShow::Vec Eye(0.0, -3000.0, 900.0);
		double SceneTook[4] = { 0.0, 0.0, 0.0, 0.0 }, ShowTook = 0.0, CompareTook = 0.0;
		for (int Fr = 0; Fr < Frames; ++Fr)
		{
			const double T = BeginT + Fr / 30.0;
			const auto Stepping = std::chrono::steady_clock::now();
			for (Scene* Sc : Scenes) Sc->Game.EyeAt = Sc->Game.World(Eye.X, Eye.Y, Eye.Z);
			const FVector Seen3 = TodayInk.Game.BoardOf(TodayInk.Game.EyeAt);
			while (Taken < Rec.Views.size() && Rec.Views[Taken - 1].Head.T <= T + 1e-9) TakeNext();
			State->Show.Eye = SeaShow::Vec(Seen3.X, Seen3.Y, std::max(Seen3.Z, 2.0));
			State->Show.Advance(T);
			State->BattleT = T;
			bool HeldBefore[4][2];
			for (int K = 0; K < 4; ++K)
			{
				HeldBefore[K][0] = Scenes[K]->Game.FoamShape.bHeld;
				HeldBefore[K][1] = Scenes[K]->Game.MarksShape.bHeld;
			}
			const auto Laying = std::chrono::steady_clock::now();
			for (int K = 0; K < 4; ++K)
			{
				const auto One = std::chrono::steady_clock::now();
				PlayFrame(*Scenes[K], Rec);
				SceneTook[K] += std::chrono::duration<double>(std::chrono::steady_clock::now() - One).count();
			}
			const auto Laid = std::chrono::steady_clock::now();
			ShowTook += std::chrono::duration<double>(Laying - Stepping).count();
			// the kinds of mark in today's own meshes (with the wake ink)
			for (int M = 0; M < 2; ++M)
			{
				const UProceduralMeshComponent& Mesh = M == 0 ? TodayInk.FoamMesh : TodayInk.MarksMesh;
				if (Mesh.ProcMeshSections.empty()) continue;
				const FProcMeshSection& Sec = Mesh.ProcMeshSections[0];
				PiecesOf(Sec.ProcIndexBuffer, Pieces);
				for (const Piece& P : Pieces)
				{
					const EKind Kind = M == 0 ? FoamKind(P, Sec) : MarksKind(P, Sec);
					Kinds[Kind] += P.Triangles;
					if (Kind == Unknown) Unknowns[std::to_string(P.Points) + " points, " + std::to_string(P.Triangles) + " triangles" + (P.Rows ? ", " + std::to_string(P.Rows) + " rows of " + std::to_string(P.Across) : "")] += P.Triangles;
				}
			}
			// kept against today's, mesh by mesh
			const UProceduralMeshComponent* Todays[4] = { &TodayInk.FoamMesh, &TodayInk.MarksMesh, &TodayBare.FoamMesh, &TodayBare.MarksMesh };
			Scene* Kept[4] = { &KeptInk, &KeptInk, &KeptBare, &KeptBare };
			for (int K = 0; K < 4; ++K)
			{
				Account& Ac = Acc[K];
				Scene& Sc = *Kept[K];
				const bool bFoam = K % 2 == 0;
				const UProceduralMeshComponent& Mine = bFoam ? Sc.FoamMesh : Sc.MarksMesh;
				const UProceduralMeshComponent& Today = *Todays[K];
				const SeaMarksPool::Shape& S = bFoam ? Sc.Game.FoamShape : Sc.Game.MarksShape;
				++Ac.Frames;
				const Scene& Was = K < 2 ? TodayInk : TodayBare;
				auto Ink = [](const Scene& Of, const UProceduralMeshComponent& Mesh)
				{
					const UMaterialInterface* M = Mesh.GetMaterial(0);
					return M == nullptr ? 0 : M == &Of.FoamInkMat ? 1 : M == &Of.MarkInkMat ? 2 : 3;
				};
				const Same Got = Compare(Today, Mine, Ink(Was, Today), Ink(Sc, Mine), A, B);
				Ac.Triangles += Got.Triangles;
				Ac.NoArea += Got.Flat;
				if (!Got.bSame && Ac.Differ++ == 0) Ac.FirstDiffer = "at " + Num(T, 2) + " s: " + Got.Why;
				if (K == 2)
				{
					// (with no wake ink DrawMarks never touches the foam's mesh)
					if ((Mine.Creates + Mine.Updates + Mine.Clears + Mine.MaterialsSet + Mine.Dirtied != 0 || !Mine.ProcMeshSections.empty()) && Ac.Broken++ == 0)
						Ac.FirstBroken = "at " + Num(T, 2) + " s: the foam's mesh touched with no wake ink";
					continue;
				}
				// today's shape, against its last frame's: whether the kept mesh should be sent again in place (decided here
				// apart from the rule's own Next)
				const int TodayPts = PointsOf(Today);
				const bool bTodayHas = TodayPts > 0;
				static const std::vector<uint32> NoIndices;
				const std::vector<uint32>& Indices = bTodayHas ? Today.ProcMeshSections[0].ProcIndexBuffer : NoIndices;
				const bool bWasHeld = HeldBefore[Kept[K] == &KeptInk ? 1 : 3][bFoam ? 0 : 1];
				const bool bShapeHeld = bTodayHas && bWasHeld && Ac.LastPoints == TodayPts && Ac.LastIndices == Indices;
				Ac.LastPoints = TodayPts;
				Ac.LastIndices = Indices;
				const std::string Broke = ShapeRules(Mine, S, bTodayHas, bShapeHeld, bWasHeld, bFoam ? Sc.FoamIndices : Sc.MarksIndices);
				if (!Broke.empty() && Ac.Broken++ == 0) Ac.FirstBroken = "at " + Num(T, 2) + " s: " + Broke;
				// the points sent and the proxies made afresh, today's (made every frame it has something) and kept
				const bool bSent = S.This == SeaMarksPool::Send::Make || S.This == SeaMarksPool::Send::Update;
				const int Sent = bSent ? PointsOf(Mine) : 0, TodaySent = Today.Creates > 0 ? TodayPts : 0;
				Ac.TodayPoints += TodaySent;
				Ac.KeptPoints += Sent;
				Ac.MostTodayPoints = std::max(Ac.MostTodayPoints, TodaySent);
				if (Sent > TodaySent && Ac.OverPoints++ == 0) Ac.FirstOver = "at " + Num(T, 2) + " s: " + std::to_string(Sent) + " points sent, today's " + std::to_string(TodaySent);
				Ac.TodayProxies += Today.Dirtied > 0 ? 1 : 0;
				Ac.KeptProxies += Mine.Dirtied > 0 ? 1 : 0;
				Ac.Made += S.This == SeaMarksPool::Send::Make ? 1 : 0;
				Ac.Updated += S.This == SeaMarksPool::Send::Update ? 1 : 0;
				Ac.Cleared += S.This == SeaMarksPool::Send::Clear ? 1 : 0;
				Ac.Idle += S.This == SeaMarksPool::Send::Nothing ? 1 : 0;
				// the engine's data path, emulated: today's made afresh every frame it has something; kept as it was sent
				if (Fr % EmuEvery == 0)
				{
					++Ac.EmuFrames;
					if (bTodayHas)
					{
						Engine::FrameOf(Today, bFoam, EmuFrame);
						if (S.This == SeaMarksPool::Send::Update && Ac.KeptSection.Points.size() != EmuFrame.V.size())
						{
							Engine::Make(Ac.KeptSection, Ac.KeptProxy, EmuFrame); // (its making fell on a frame not sampled: made here, untimed)
						}
						const auto T0 = std::chrono::steady_clock::now();
						Engine::Make(Ac.TodaySection, Ac.TodayProxy, EmuFrame);
						const auto T1 = std::chrono::steady_clock::now();
						if (S.This == SeaMarksPool::Send::Make) Engine::Make(Ac.KeptSection, Ac.KeptProxy, EmuFrame);
						else if (S.This == SeaMarksPool::Send::Update) Engine::Update(Ac.KeptSection, Ac.KeptProxy, EmuFrame);
						const auto T2 = std::chrono::steady_clock::now();
						Ac.TodayEmu += std::chrono::duration<double>(T1 - T0).count();
						Ac.KeptEmu += std::chrono::duration<double>(T2 - T1).count();
					}
				}
			}
			Eye = NextEye(State->Show);
			CompareTook += std::chrono::duration<double>(std::chrono::steady_clock::now() - Laid).count();
		}
		const double Took = std::chrono::duration<double>(std::chrono::steady_clock::now() - Began).count();
		Note("played", std::to_string(Frames) + " frames (" + Num((Frames - 1) / 30.0, 0) + " s at 30 a second, to 12 s past the last view), four scenes, in " + Num(Took, 1) + " s: the show " + Num(ShowTook, 1) +
			" s; today's and kept, with ink " + Num(SceneTook[0], 1) + " and " + Num(SceneTook[1], 1) + " s, without " + Num(SceneTook[2], 1) + " and " + Num(SceneTook[3], 1) +
			" s; held to each other " + Num(CompareTook, 1) + " s");
		std::string Found;
		for (int K = 0; K < KindCount; ++K) Found += std::string(K ? "; " : "") + KindNames[K] + " " + std::to_string(Kinds[K]);
		Note("the kinds of mark found in today's own meshes, with the wake ink (triangles over every frame)", Found);
		if (!Unknowns.empty())
		{
			std::vector<std::pair<long long, std::string>> Most;
			for (const auto& [Shape, Count] : Unknowns) Most.emplace_back(Count, Shape);
			std::sort(Most.rbegin(), Most.rend());
			std::string Said;
			for (size_t K = 0; K < Most.size() && K < 3; ++K) Said += std::string(K ? "; " : "") + Most[K].second + ": " + std::to_string(Most[K].first);
			Note("of no kind known, the most (pieces of)", Said);
		}
		for (int K = 0; K < 4; ++K)
		{
			const Account& Ac = Acc[K];
			Check(std::string("every frame, kept draws today's: ") + Ac.Name, Ac.Differ == 0,
				Ac.Differ == 0 ? std::to_string(Ac.Frames) + " frames, " + std::to_string(Ac.Triangles) + " triangles, the same three points each in order (" + std::to_string(Ac.NoArea) +
						" of no area, as laid)"
							   : std::to_string(Ac.Differ) + " frames differ, first " + Ac.FirstDiffer);
			if (K == 2)
			{
				Check("with no wake ink, the foam's own mesh never touched, kept or today's", Ac.Broken == 0, Ac.Broken == 0 ? "untouched" : Ac.FirstBroken);
				continue;
			}
			Check(std::string("each frame, step 1(a)'s rule: made afresh exactly when its shape changed, sent again in place while it held, cleared once: ") + Ac.Name, Ac.Broken == 0,
				Ac.Broken == 0 ? "made " + std::to_string(Ac.Made) + ", sent again " + std::to_string(Ac.Updated) + ", cleared " + std::to_string(Ac.Cleared) + ", nothing sent " + std::to_string(Ac.Idle)
							   : std::to_string(Ac.Broken) + " frames broke it, first " + Ac.FirstBroken);
			Check(std::string("no frame sends more points kept than today's (today's own or none), nor makes its proxy afresh more often: ") + Ac.Name,
				Ac.OverPoints == 0 && Ac.KeptPoints <= Ac.TodayPoints && Ac.KeptProxies <= Ac.TodayProxies,
				Ac.OverPoints == 0 ? Num(static_cast<double>(Ac.KeptPoints) / std::max<long long>(1, Ac.Frames), 0) + " points a frame against today's " +
						Num(static_cast<double>(Ac.TodayPoints) / std::max<long long>(1, Ac.Frames), 0) + "; proxies made afresh " + std::to_string(Ac.KeptProxies) + " against today's " +
						std::to_string(Ac.TodayProxies)
								   : std::to_string(Ac.OverPoints) + " frames over, first " + Ac.FirstOver);
			Note(std::string("what it saves: ") + Ac.Name,
				"its render proxy kept " + std::to_string(Ac.TodayProxies - Ac.KeptProxies) + " of today's " + std::to_string(Ac.TodayProxies) + " makings (" +
					Share(Ac.TodayProxies - Ac.KeptProxies, Ac.TodayProxies) + "; sent again in place " + Share(Ac.Updated, Ac.Frames - Ac.Idle - Ac.Cleared) +
					" of the frames with something to draw); the engine's per-point data path, emulated on every " + std::to_string(EmuEvery) + "th frame (" +
					std::to_string(Ac.EmuFrames) + "): today's " + Num(1000.0 * Ac.TodayEmu / std::max<long long>(1, Ac.EmuFrames), 3) + " ms a frame, kept " +
					Num(1000.0 * Ac.KeptEmu / std::max<long long>(1, Ac.EmuFrames), 3) + " ms (the cloud Xeon; the proxy's own making and the scene's not in it)");
		}
		Note("the meshes' line, as the game says it", SeaMarksPool::Line(KeptInk.Game.FoamShape, KeptInk.Game.MarksShape));
		for (int K = 0; K < KindCount; ++K) All.Kinds[K] += Kinds[K];
		const int Drawn[3] = { 0, 1, 3 };
		for (int M = 0; M < 3; ++M)
		{
			All.TodayPoints[M] += Acc[Drawn[M]].TodayPoints;
			All.KeptPoints[M] += Acc[Drawn[M]].KeptPoints;
			All.TodayProxies[M] += Acc[Drawn[M]].TodayProxies;
			All.KeptProxies[M] += Acc[Drawn[M]].KeptProxies;
		}
		All.Frames += Frames;
	}

	// ---------------------------------------------------------------------------------------------------------------
	// The rule's own cases, on made-up triangles.

	void Rules()
	{
		using namespace SeaMarksPool;
		std::printf("The rule's own cases (Battle/SeaMarksPool.h)\n");
		{
			Shape S;
			const int32_t Quad[] = { 0, 1, 2, 0, 2, 3 }, Turned[] = { 0, 2, 1, 0, 3, 2 }, Two[] = { 0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7 };
			const Send First = Next(S, Quad, 6, 4);
			const Send Again = Next(S, Quad, 6, 4);
			const Send Other = Next(S, Turned, 6, 4);
			const Send More = Next(S, Quad, 6, 5);
			const Send Grown = Next(S, Two, 12, 8);
			const Send Held = Next(S, Two, 12, 8);
			const Send Part = Next(S, Two, 11, 8);
			Check("made at first; sent again in place while the same points and triangles hold; made afresh when a triangle turns, the points' count changes, or "
				  "triangles come or go",
				First == Send::Make && Again == Send::Update && Other == Send::Make && More == Send::Make && Grown == Send::Make && Held == Send::Update && Part == Send::Make &&
					S.Made == 5 && S.Updated == 2 && S.Points == 8 && S.Tris.size() == 11,
				"made " + std::to_string(S.Made) + ", sent again " + std::to_string(S.Updated));
		}
		{
			Shape S, Never;
			const int32_t Quad[] = { 0, 1, 2, 0, 2, 3 };
			Next(S, Quad, 6, 4);
			const Send Cleared = Next(S, Quad, 0, 0);
			const Send Idle = Next(S, Quad, 0, 0);
			const Send Back = Next(S, Quad, 6, 4);
			const Send None = Next(Never, Quad, 0, 0);
			Check("nothing to draw: cleared once, then nothing sent; the same shape after it made afresh (the section was cleared); nothing ever drawn, nothing sent",
				Cleared == Send::Clear && Idle == Send::Nothing && Back == Send::Make && None == Send::Nothing && !Never.bHeld && S.Cleared == 1 && S.Idle == 1 && Never.Idle == 1 &&
					S.PointsSent == 8 && S.MostPoints == 4 && S.Frames == 4,
				"cleared " + std::to_string(S.Cleared) + ", idle " + std::to_string(S.Idle));
		}
		{
			Shape Foam, Marks;
			Foam.Frames = 10;
			Foam.Made = 2;
			Foam.Updated = 7;
			Foam.Cleared = 1;
			Foam.Idle = 0;
			Foam.PointsSent = 23549 * 9;
			Foam.MostPoints = 31000;
			const std::string Said = Line(Foam, Marks);
			Check("the meshes' line once a battle, in words",
				Said == "Sea marks pool: the foam's mesh sent again in place 7 of 10 frames, made afresh 2, cleared 1, sent nothing 0 (21194 points a frame on the mean, at most 31000: "
						"today's own); the marks' mesh sent again in place 0 of 0 frames, made afresh 0, cleared 0, sent nothing 0 (0 points a frame on the mean, at most 0: today's own).",
				Said);
			std::string Lower = Said;
			for (char& C : Lower) C = static_cast<char>(std::tolower(static_cast<unsigned char>(C)));
			Check("no word of the meshes' line says dice", !Has(Lower, "dice"), "the line");
		}
		{
			// the kinds' own reading, on a made-up mesh: a ribbon of 3 rows, five across (8 triangles a row), then a ring of
			// three rings of 8 (16 a ring)
			std::vector<uint32> T;
			for (int R = 0; R < 2; ++R)
				for (int C = 0; C < 4; ++C)
				{
					const uint32 A0 = static_cast<uint32>(R * 5 + C), B0 = A0 + 5;
					for (const uint32 K : { A0, A0 + 1, B0 + 1, A0, B0 + 1, B0 }) T.push_back(K);
				}
			for (int R = 0; R < 2; ++R)
				for (int K = 0; K < 8; ++K)
				{
					const uint32 A0 = static_cast<uint32>(15 + R * 8 + K), A1 = static_cast<uint32>(15 + R * 8 + (K + 1) % 8);
					for (const uint32 J : { A0, A1, A1 + 8, A0, A1 + 8, A0 + 8 }) T.push_back(J);
				}
			std::vector<Piece> Got;
			PiecesOf(T, Got);
			Check("a mesh cut into its pieces, each by its rows: a ribbon of three rows five across, and a man's ring of three rings of eight",
				Got.size() == 2 && Got[0].First == 0 && Got[0].Points == 15 && Got[0].Across == 5 && Got[0].Rows == 3 && Got[0].Triangles == 16 && Got[1].First == 15 &&
					Got[1].Points == 24 && Got[1].Across == 8 && Got[1].Rows == 3 && Got[1].Triangles == 32,
				std::to_string(Got.size()) + " pieces");
		}
	}
}

namespace
{
	void Beside(const std::string& Src)
	{
		std::printf("The game's own lines this stands beside\n");
		{
			std::map<std::string, std::string> Texts;
			int Found = 0;
			std::string Missing;
			for (const Pin& P : Pins)
			{
				if (!Texts.count(P.File)) Texts[P.File] = ReadAll(Src + "/" + P.File);
				if (Has(Texts[P.File], P.Line)) ++Found;
				else Missing += std::string(Missing.empty() ? "" : " | ") + P.File + ": " + P.Line;
			}
			const int All = static_cast<int>(sizeof(Pins) / sizeof(Pins[0]));
			Check("the lines mirrored or declared here still read so in the game's source", Found == All, std::to_string(Found) + " of " + std::to_string(All) + (Missing.empty() ? "" : "; missing " + Missing));
			const std::string& Cpp = Texts["Battle/SeaBattle.cpp"];
			const std::string Draw = Body(Cpp, "void ASeaBattle::DrawMarks()");
			const size_t Pooled = Draw.find("\tif (bMarksPool)\n\t{\n\t\tDrawMarksPooled(FV, FN, FU, FC, FT, DV, DN, DC, DT, bInk);\n\t\treturn;\n\t}\n\tif (Foam && bInk)\n");
			const std::string After = Pooled == std::string::npos ? std::string() : Draw.substr(Pooled);
			Check("DrawMarks asks for its meshes kept once, after every mark is laid and before today's two meshes are made, and goes no further",
				Pooled != std::string::npos && CountOf(Draw, "bMarksPool") == 1 && !Has(After, "Foamy(") && !Has(After, "Quad(") && !Has(After, "Ribbon(") && !Has(After, "DV.Add(") &&
					Has(After, "Foam->CreateMeshSection(0, FV, FT, FN, FU, FC, TArray<FProcMeshTangent>(), false);") &&
					Has(After, "Marks->CreateMeshSection(0, DV, DT, DN, TArray<FVector2D>(), DC, TArray<FProcMeshTangent>(), false);"),
				Pooled == std::string::npos ? "its lines not found" : "asked " + std::to_string(CountOf(Draw, "bMarksPool")) + " time(s)");
			const std::string Sent = Body(Cpp, "void ASeaBattle::PoolMarks(");
			Check("a kept mesh made only with CreateMeshSection (today's arrays), sent again only with UpdateMeshSection (no normals: as made), cleared only with ClearMeshSection, "
				  "each once; never a still mesh",
				CountOf(Sent, "Mesh->CreateMeshSection(0, V, T, N, U, C, TArray<FProcMeshTangent>(), false);") == 1 &&
					CountOf(Sent, "Mesh->UpdateMeshSection(0, V, TArray<FVector>(), U, C, TArray<FProcMeshTangent>());") == 1 && CountOf(Sent, "Mesh->ClearMeshSection(0);") == 1 &&
					CountOf(Sent, "MeshSection(") == 3 && !Has(Sent + Body(Cpp, "void ASeaBattle::DrawMarksPooled("), "TheaterStillMesh"),
				"PoolMarks");
			Check("DrawMarks lays every normal straight up (so the kept mesh's normals, as made, stay today's)",
				CountOf(Draw, "FN.Add(") == 1 && Has(Draw, "FN.Add(FVector::UpVector);") && CountOf(Draw, "DN.Add(") == 1 && Has(Draw, "DN.Add(FVector::UpVector);"), "FN, DN");
			Check("the default is today's (off), read once as the scene opens, the meshes' line said once a battle with the threads' and the \"Sea look:\" line",
				Has(Body(Cpp, "bool ASeaBattle::OpenOn("), "bMarksPool = SeaLookSwitches::TakeOnOff(SeaLook, \"TheaterSeaMarksPool=\");") &&
					Has(Body(Cpp, "void ASeaBattle::SayThreadsAndLook() const"), "if (bMarksPool)") && Has(Texts["Battle/SeaBattle.h"], "bool bMarksPool = false;"),
				"OpenOn, SayThreadsAndLook");
		}
	}

	void Sum(const Tally& All, int Played, int Passed, int Wanted)
	{
		std::printf("Over every recording and stand-in\n");
		Check("every recording and stand-in played, each passing", Played == Wanted && Passed == Played && Played >= 9, std::to_string(Passed) + " of " + std::to_string(Played) + " passed, " + std::to_string(Wanted) + " given");
		bool bEvery = true;
		std::string Found;
		for (int K = 0; K < Unknown; ++K)
		{
			bEvery = bEvery && All.Kinds[K] > 0;
			Found += std::string(K ? "; " : "") + KindNames[K] + " " + std::to_string(All.Kinds[K]);
		}
		Check("every kind of mark DrawMarks lays found in today's own meshes and drawn kept (triangles over every frame, each kind by its shape and paint)", bEvery, Found);
		long long Known = 0;
		for (int K = 0; K < Unknown; ++K) Known += All.Kinds[K];
		Note("triangles of no kind known (a ribbon or ring whose rows are not alike: laid the other way round part way, or too short to tell)",
			std::to_string(All.Kinds[Unknown]) + " of " + std::to_string(Known + All.Kinds[Unknown]));
		const char* Names[3] = { "the foam, on the wake ink", "the marks (the oil), with the wake ink", "the marks (the oil and the foam), with no wake ink" };
		bool bGate = true;
		for (int M = 0; M < 3; ++M)
		{
			bGate = bGate && All.KeptPoints[M] <= All.TodayPoints[M] && All.KeptProxies[M] <= All.TodayProxies[M];
			Note(std::string("over them all: ") + Names[M],
				"points a frame " + Num(static_cast<double>(All.KeptPoints[M]) / std::max<long long>(1, All.Frames), 0) + " kept, " +
					Num(static_cast<double>(All.TodayPoints[M]) / std::max<long long>(1, All.Frames), 0) + " today's; render proxies made afresh " + std::to_string(All.KeptProxies[M]) +
					" kept, " + std::to_string(All.TodayProxies[M]) + " today's (" + Share(All.TodayProxies[M] - All.KeptProxies[M], All.TodayProxies[M]) + " kept)");
		}
		Check("over them all, no more points sent kept than today's, and no more render proxies made afresh", bGate, "the three meshes drawn");
	}

	int Done()
	{
		std::printf("%d checks, %d failed\n%s\n", Checks, Failed, Failed ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED");
		return Failed ? 1 : 0;
	}

	void WriteTally(std::ofstream& Out, const std::string& Path, bool bOk, const Tally& One)
	{
		Out << "kinds\t" << Path << '\t' << (bOk ? 1 : 0);
		for (const long long K : One.Kinds) Out << '\t' << K;
		for (int M = 0; M < 3; ++M) Out << '\t' << One.TodayPoints[M] << '\t' << One.KeptPoints[M] << '\t' << One.TodayProxies[M] << '\t' << One.KeptProxies[M];
		Out << '\t' << One.Frames << '\n';
	}

	bool ReadTally(const std::string& Text, bool& bOk, Tally& One)
	{
		std::istringstream Line(Text);
		std::string Tag, Path;
		int Ok = 0;
		if (!std::getline(Line, Tag, '\t') || Tag != "kinds" || !std::getline(Line, Path, '\t') || !(Line >> Ok)) return false;
		for (long long& K : One.Kinds)
			if (!(Line >> K)) return false;
		for (int M = 0; M < 3; ++M)
			if (!(Line >> One.TodayPoints[M] >> One.KeptPoints[M] >> One.TodayProxies[M] >> One.KeptProxies[M])) return false;
		if (!(Line >> One.Frames)) return false;
		bOk = Ok == 1;
		return true;
	}
}

int main(int ArgC, char** ArgV)
{
	// marks_pool_test SRC recording.json ...         the pins, the rule's cases and every recording, one after another
	// marks_pool_test SRC --one KINDS recording.json  one recording alone, its tally written to KINDS (run.sh: four at once)
	// marks_pool_test SRC --sum KINDS ...             the pins, the rule's cases and the tallies of every recording played
	if (ArgC < 3)
	{
		std::printf("usage: marks_pool_test <Source/TheaterCommand> recording.json ... | --one KINDS recording.json | --sum KINDS ...\n");
		return 2;
	}
	const std::string Src = ArgV[1], Mode = ArgV[2];
	if (Mode == "--one")
	{
		if (ArgC != 5) return 2;
		Tally One;
		Play(ArgV[4], One);
		std::ofstream Out(ArgV[3]);
		WriteTally(Out, ArgV[4], Failed == 0, One);
		return Done();
	}
	std::printf("DrawMarks' meshes kept (the release 2 ocean plan, step 1(a))\n");
	Beside(Src);
	Rules();
	Tally All;
	int Played = 0, Passed = 0, Wanted = 0;
	if (Mode == "--sum")
	{
		for (int K = 3; K < ArgC; ++K)
		{
			++Wanted;
			Tally One;
			bool bOk = false;
			if (ReadTally(ReadAll(ArgV[K]), bOk, One))
			{
				++Played;
				Passed += bOk ? 1 : 0;
				for (int J = 0; J < KindCount; ++J) All.Kinds[J] += One.Kinds[J];
				for (int M = 0; M < 3; ++M)
				{
					All.TodayPoints[M] += One.TodayPoints[M];
					All.KeptPoints[M] += One.KeptPoints[M];
					All.TodayProxies[M] += One.TodayProxies[M];
					All.KeptProxies[M] += One.KeptProxies[M];
				}
				All.Frames += One.Frames;
			}
		}
	}
	else
	{
		for (int K = 2; K < ArgC; ++K)
		{
			++Wanted;
			const int Before = Failed;
			Play(ArgV[K], All);
			++Played;
			Passed += Failed == Before ? 1 : 0;
		}
	}
	Sum(All, Played, Passed, Wanted);
	return Done();
}
