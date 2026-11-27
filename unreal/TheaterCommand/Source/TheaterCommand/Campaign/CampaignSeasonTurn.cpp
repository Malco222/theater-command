#include "Campaign/CampaignMapActor.h"

#include "Campaign/CampaignTextures.h"
#include "Campaign/SeasonTurn.h"
#include "Campaign/TheaterSpeed.h"
#include "TheaterCommand.h"
#include "Async/Async.h"
#include "Async/ParallelFor.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "InputCoreTypes.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

#include <atomic>
#include <vector>

/*
 * (Release 2, the campaign map look builder: the season turns. Brian's pick of 18 November 2026, "a season turns
 * moment on the campaign map, where snow or mud visibly spreads across the land at the start of winter or spring";
 * the reckoning, and its cloud test, are in Campaign/SeasonTurn.h.) The table's side of it:
 *
 *  - The mud. ShowMud hands the rules' mud ground to AskSeasonMud in place of the flat brown wash. A fifth of a second
 *    later, in a frame of its own, the ground round it is copied out (the box of the mud's provinces and some way
 *    beyond, for the water near them), and another thread works out where the mud lies, how wet, when the soak
 *    reaches each point, and the ground's colour with it: the frame that takes the answer only swaps arrays. The mud
 *    is painted into the ground picture, under the owners' stain, the fields, the woods and the borders, as the art
 *    director asked, and only in the rules' own mud ground: no nation and no owner is read here.
 *  - The snow. When the month has stepped on by one in play and the snow changes, SetSeason hands the month's new
 *    masks to TakeSeasonSnow instead of sending them: the old month's snow stays on the table until the turn, and the
 *    new spreads over it from the north and the high ground (at the thaw it goes from the south and the low ground).
 *  - The turn. Three quarters of a second after the month steps on (the month card comes first, and the frame of the
 *    month's turn is a heavy one), the change spreads over about three and a half seconds; any key or click settles it
 *    in a fifth of a second. Each frame of it, at most thirty a second, paints only the squares of the picture (128
 *    pixels a side) the front is crossing and sends just those, with their first three smaller copies; once settled,
 *    the whole picture is sent once, as SetSeason always sent it, in a frame of its own.
 *
 * Off (tc.Map.SeasonTurn 0, as it comes), nothing here runs but the switch's own read each frame, and the table is
 * as before: the brown wash, and the snow at once. Turned on or off in play, the mud is laid or taken up again at once.
 * Only the map's own sheet takes the mud; ground on the wider chart's sheets keeps none (none of the rules' mud lies
 * there in the east's months).
 */

namespace
{
	TAutoConsoleVariable<int32> CVarSeasonTurn(
		TEXT("tc.Map.SeasonTurn"), 0,
		TEXT("1: the mud lies in the ground itself, soaking out from the rivers and the low ground and stopping inside the rules' ground, and when a month brings or takes snow or mud the change spreads over the land in about three and a half seconds (any key or click settles it); 0: the flat brown wash, and the snow at once, as before. Release 2; off as it comes."));
	TAutoConsoleVariable<int32> CVarSeasonTurnPlay(
		TEXT("tc.Map.SeasonTurnPlay"), 1,
		TEXT("With tc.Map.SeasonTurn: 1 the month's change spreads over the land; 0 it is simply there."));
	TAutoConsoleVariable<float> CVarSeasonTurnMuch(
		TEXT("tc.Map.SeasonTurnMuch"), 1.0f,
		TEXT("With tc.Map.SeasonTurn: how heavy the mud lies, 0 (none) to 1.5. Read when the mud is next laid."));
	TAutoConsoleVariable<float> CVarSeasonTurnReplay(
		TEXT("tc.Map.SeasonTurnReplay"), 0.0f,
		TEXT("With tc.Map.SeasonTurn: set it to a new number above nought to play this month's turn again from the month before (its snow, and mud that soaks in), for pictures."));
	TAutoConsoleVariable<float> CVarSeasonTurnAt(
		TEXT("tc.Map.SeasonTurnAt"), -1.0f,
		TEXT("With tc.Map.SeasonTurn: 0 or more holds every turn this many seconds after its spread began, for stills (1 is about a third of the way, 2 about two thirds, 3.5 settled); -1 lets it run."));

	bool SeasonTurnWanted()
	{
		static const bool bAsked = FParse::Param(FCommandLine::Get(), TEXT("TheaterSeasonTurn"));
		return bAsked || CVarSeasonTurn.GetValueOnGameThread() != 0;
	}

	/** Any key at all, or any button of the mouse, pressed this frame. */
	bool SeasonTurnKeyPressed(UWorld* World)
	{
		APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
		if (!Controller)
		{
			return false;
		}
		static TArray<FKey> Every;
		if (Every.Num() == 0)
		{
			EKeys::GetAllKeys(Every);
			Every.RemoveAll([](const FKey& Key) { return Key.IsAxis1D() || Key.IsAxis2D() || Key.IsAxis3D() || Key.IsGamepadKey() || Key.IsTouch() || Key == EKeys::AnyKey; });
		}
		for (const FKey& Key : Every)
		{
			if (Controller->WasInputKeyJustPressed(Key))
			{
				return true;
			}
		}
		return false;
	}
}

/** All the season turn keeps between frames (ACampaignMapActor::SeasonTurn). */
struct FSeasonTurnTable
{
	/** A square of a picture a turn paints, and what changes in it. */
	struct FTile
	{
		int32 X0 = 0, Y0 = 0, X1 = -1, Y1 = -1; // its pixels, inclusive
		float First = 1.0f, Last = 0.0f; // the earliest and the latest arrival in it
		TArray<int32> Cells; // the pixels that change, as places in the whole picture, row by row
		TArray<float> Arrive; // when each changes: 0 the first to, 1 the last
		TArray<uint8> SnowWas, SnowWill; // (snow) the green of the masks before and after
		TArray<FColor> Whole, Banded; // (mud) the ground with the mud on it whole, and with the wet band on it too

		void Note(float A)
		{
			First = FMath::Min(First, A);
			Last = FMath::Max(Last, A);
		}
	};
	static constexpr int32 TileSide = 128;

	/** The squares of a picture Wide by High, each with its box and nothing in it. */
	static void MakeTiles(int32 Wide, int32 High, TArray<FTile>& Tiles)
	{
		const int32 Across = FMath::DivideAndRoundUp(Wide, TileSide), Down = FMath::DivideAndRoundUp(High, TileSide);
		Tiles.Reset();
		Tiles.SetNum(Across * Down);
		for (int32 TY = 0; TY < Down; ++TY)
		{
			for (int32 TX = 0; TX < Across; ++TX)
			{
				FTile& Tile = Tiles[TY * Across + TX];
				Tile.X0 = TX * TileSide;
				Tile.Y0 = TY * TileSide;
				Tile.X1 = FMath::Min(Wide, (TX + 1) * TileSide) - 1;
				Tile.Y1 = FMath::Min(High, (TY + 1) * TileSide) - 1;
			}
		}
	}

	/** A turn that is spreading: its clock. */
	struct FPlay
	{
		bool bOn = false;
		bool bBackward = false; // (mud drying: from its far edges back to the water, and no wet band)
		double Began = 0.0; // the time its spread begins (FPlatformTime, seconds)
		double Painted = -1.0; // where its front stood when it was last painted
		double SkipAt = -1.0, SkipFrom = 0.0; // a key or click: when, and where the front stood then

		double FrontAt(double Now, double Held) const
		{
			if (Held >= 0.0)
			{
				return TheaterSeasonTurn::Front(Held);
			}
			double Stands = TheaterSeasonTurn::Front(Now - Began);
			if (SkipAt >= 0.0)
			{
				const double T = FMath::Clamp((Now - SkipAt) / TheaterSeasonTurn::SkipTakes, 0.0, 1.0);
				Stands = FMath::Max(Stands, FMath::Lerp(SkipFrom, TheaterSeasonTurn::Settled, T));
			}
			return Stands;
		}
	};

	/** The mud, worked out on another thread: what it is given (a copy: it never touches the map), and what it gives back. */
	struct FMudWork
	{
		int32 Gen = 0;
		bool bTurn = false;
		int32 Wide = 0, High = 0; // the whole picture
		int32 I0 = 0, J0 = 0, NX = 0, NY = 0; // the box it works in
		double Cell = 1.5, OriginX = 0.0, OriginY = 0.0;
		float Much = 1.0f;
		TArray<uint8> Kind; // by point of the box: 0 land the mud does not lie on, 1 the rules' mud ground, 2 water
		TArray<float> Height, Rough;
		TArray<FColor> Ground;
		TArray<TArray<FVector2D>> Waters; // the rivers' courses and the lakes' shores near it, in map units
		TArray<FTile> Tiles; // (back) the whole picture's squares, those in the box filled
		int32 Muddy = 0;
		double Took = 0.0; // (for the log) seconds of work
		std::atomic<bool> bDone{ false };

		void Work();
	};

	bool bWasOn = false;
	// The season.
	int32 Month = -1;
	float Hard = 0.0f, HardBefore = 0.0f;
	double StepAt = -1000.0; // when the month last stepped on by one in play
	bool bStepNow = false; // (between NoteSeason and TakeSeasonSnow, in the same SetSeason)
	double SkippedAt = -1000.0; // when a key or click last settled a turn

	// The snow.
	TArray<FColor> Masks; // the masks picture being turned (whole): the new month's, with what has not yet turned as it was
	bool bSnowTaken = false; // the new snow handed over, its squares not yet laid
	double SnowTakenAt = 0.0;
	TArray<FTile> SnowTiles;
	FPlay Snow;

	// The mud.
	TArray<int32> MudAsked;
	bool bMudAsk = false;
	bool bMudRush = false; // (asked while a turn of the mud was under way: laid at once)
	double MudAskedAt = 0.0;
	int32 MudGen = 0;
	TSharedPtr<FMudWork, ESPMode::ThreadSafe> MudWork;
	TFuture<void> MudBuilding;
	TArray<FColor> Ground; // the ground picture as it is sent (whole): empty while no mud lies
	TArray<FTile> MudTiles; // the mud that lies (or is soaking in, or drying)
	FPlay Mud;

	// Sending.
	bool bSendMasks = false, bSendGround = false; // the whole picture, once, in a frame of its own
	double LastPaint = -1000.0;
	float ReplayPlayed = 0.0f;

	bool MudLies() const { return MudTiles.Num() > 0; }
	bool Busy() const { return bSnowTaken || Snow.bOn || bMudAsk || MudWork.IsValid() || Mud.bOn || bSendMasks || bSendGround; }

	/** A turn begins: three quarters of a second after the month stepped on (or now, if that has passed); at once, if a key or click has just settled the other. */
	void Begin(FPlay& Play, double Now, bool bBackward) const
	{
		Play = FPlay();
		Play.bOn = true;
		Play.bBackward = bBackward;
		Play.Began = FMath::Max(Now, StepAt + TheaterSeasonTurn::Delay);
		if (Now - SkippedAt < 4.0)
		{
			Play.SkipAt = Now;
			Play.SkipFrom = 0.0;
		}
	}

	/** A turn of the mud under way is brought to its end at once (the whole picture is sent in a later frame). */
	void SettleMud(const TArray<FColor>& DryGround)
	{
		if (!Mud.bOn || Ground.Num() != DryGround.Num())
		{
			Mud = FPlay();
			return;
		}
		const bool bDrying = Mud.bBackward;
		for (const FTile& Tile : MudTiles)
		{
			for (int32 C = 0; C < Tile.Cells.Num(); ++C)
			{
				Ground[Tile.Cells[C]] = bDrying ? DryGround[Tile.Cells[C]] : Tile.Whole[C];
			}
		}
		if (bDrying)
		{
			MudTiles.Empty();
		}
		Mud = FPlay();
		bSendGround = true;
	}

	/** The mud that lies is taken up from the ground picture (the whole picture is sent in a later frame). */
	void LiftMud(const TArray<FColor>& DryGround)
	{
		if (Ground.Num() == DryGround.Num())
		{
			for (const FTile& Tile : MudTiles)
			{
				for (const int32 Cell : Tile.Cells)
				{
					Ground[Cell] = DryGround[Cell];
				}
			}
			bSendGround = true;
		}
		MudTiles.Empty();
		Mud = FPlay();
	}

	/** A turn of the snow under way, or handed over and not yet laid, is put by: its masks are the new month's again. */
	void SettleSnow()
	{
		if (Snow.bOn)
		{
			for (const FTile& Tile : SnowTiles)
			{
				for (int32 C = 0; C < Tile.Cells.Num(); ++C)
				{
					Masks[Tile.Cells[C]].G = Tile.SnowWill[C];
				}
			}
		}
		SnowTiles.Empty();
		bSnowTaken = false;
		Snow = FPlay();
	}
};

void FSeasonTurnTable::FMudWork::Work()
{
	const double Start = FPlatformTime::Seconds();
	const int32 Count = NX * NY;
	auto Box = [this](int32 GI, int32 GJ) { return (GJ - J0) * NX + (GI - I0); };
	// How far each point lies from water, in steps of the grid: the sea and the lakes' own water, and the rivers'
	// courses and the lakes' shores drawn in.
	const float FarWater = static_cast<float>(400.0 / Cell);
	std::vector<float> Water(static_cast<size_t>(Count), FarWater);
	for (int32 K = 0; K < Count; ++K)
	{
		if (Kind[K] == 2)
		{
			Water[static_cast<size_t>(K)] = 0.0f;
		}
	}
	for (const TArray<FVector2D>& Line : Waters)
	{
		for (int32 P = 0; P < Line.Num(); ++P)
		{
			const FVector2D A = Line[P], B = Line[FMath::Min(P + 1, Line.Num() - 1)];
			const int32 Steps = FMath::Max(1, FMath::CeilToInt32(FVector2D::Distance(A, B) / (0.5 * Cell)));
			for (int32 S = 0; S <= Steps; ++S)
			{
				const FVector2D Q = FMath::Lerp(A, B, static_cast<double>(S) / Steps);
				const int32 GI = FMath::RoundToInt32((Q.X - OriginX) / Cell), GJ = FMath::RoundToInt32((Q.Y - OriginY) / Cell);
				if (GI >= I0 && GI < I0 + NX && GJ >= J0 && GJ < J0 + NY)
				{
					Water[static_cast<size_t>(Box(GI, GJ))] = 0.0f;
				}
			}
		}
	}
	TheaterSeasonTurn::Spread(Water, NX, NY, FarWater);
	// How far each point lies from land the mud does not lie on (the edge of the rules' ground: the sea is no edge).
	const float FarEdge = static_cast<float>((TheaterSeasonTurn::EdgeStop + TheaterSeasonTurn::EdgeReach + 3.0) / Cell);
	std::vector<float> Edge(static_cast<size_t>(Count), FarEdge);
	for (int32 K = 0; K < Count; ++K)
	{
		if (Kind[K] == 0)
		{
			Edge[static_cast<size_t>(K)] = 0.0f;
		}
	}
	TheaterSeasonTurn::Spread(Edge, NX, NY, FarEdge);

	// The heights smoothed over a few units: the table's own carry a fine tooth, which would fray the soak's front
	// point by point. (Its roughness is smoothed across the provinces' borders already.)
	std::vector<float> Lie(Height.GetData(), Height.GetData() + Count);
	TheaterSeasonTurn::Blur(Lie, NX, NY, 3);

	// Each square of the picture the box reaches is worked out for itself, its points row by row.
	MakeTiles(Wide, High, Tiles);
	const int32 Across = FMath::DivideAndRoundUp(Wide, TileSide);
	const int32 TX0 = I0 / TileSide, TX1 = (I0 + NX - 1) / TileSide, TY0 = J0 / TileSide, TY1 = (J0 + NY - 1) / TileSide;
	const int32 Span = TX1 - TX0 + 1;
	std::atomic<int32> Found{ 0 };
	ParallelFor(Span * (TY1 - TY0 + 1), [&](int32 K)
	{
		FTile& Tile = Tiles[(TY0 + K / Span) * Across + TX0 + K % Span];
		for (int32 GJ = FMath::Max(Tile.Y0, J0); GJ <= FMath::Min(Tile.Y1, J0 + NY - 1); ++GJ)
		{
			for (int32 GI = FMath::Max(Tile.X0, I0); GI <= FMath::Min(Tile.X1, I0 + NX - 1); ++GI)
			{
				const int32 At = Box(GI, GJ);
				if (Kind[At] != 1)
				{
					continue;
				}
				TheaterSeasonTurn::FMudIn In;
				In.X = OriginX + GI * Cell;
				In.Y = OriginY + GJ * Cell;
				In.Water = Water[static_cast<size_t>(At)] * Cell;
				In.Edge = Edge[static_cast<size_t>(At)] * Cell;
				In.Height = Lie[static_cast<size_t>(At)];
				In.Rough = Rough[At];
				In.Wooded = Ground[At].A / 255.0;
				TheaterSeasonTurn::FMud Here = TheaterSeasonTurn::Mud(In);
				Here.Wet = FMath::Min(1.0f, Here.Wet * Much);
				Here.Pool *= FMath::Min(1.0f, Much);
				if (!(Here.Wet > 0.0f))
				{
					continue;
				}
				const FColor Dry = Ground[At];
				const uint8 From[3] = { Dry.R, Dry.G, Dry.B };
				uint8 Whole[3], Banded[3];
				TheaterSeasonTurn::PaintMud(From, Here, TheaterSeasonTurn::FShown{ 1.0f, 0.0f }, Whole);
				TheaterSeasonTurn::PaintMud(From, Here, TheaterSeasonTurn::FShown{ 1.0f, 1.0f }, Banded);
				const FColor WholeColour(Whole[0], Whole[1], Whole[2], Dry.A), BandedColour(Banded[0], Banded[1], Banded[2], Dry.A);
				if (WholeColour == Dry && BandedColour == Dry)
				{
					continue;
				}
				Tile.Cells.Add(GJ * Wide + GI);
				Tile.Arrive.Add(Here.Arrive);
				Tile.Whole.Add(WholeColour);
				Tile.Banded.Add(BandedColour);
				Tile.Note(Here.Arrive);
			}
		}
		Found.fetch_add(Tile.Cells.Num());
	});
	Muddy = Found.load();
	// The arrivals spread evenly over the turn, so that the soak crosses about as much land in each moment of it.
	std::vector<float> All;
	All.reserve(static_cast<size_t>(Muddy));
	for (const FTile& Tile : Tiles)
	{
		All.insert(All.end(), Tile.Arrive.GetData(), Tile.Arrive.GetData() + Tile.Arrive.Num());
	}
	TheaterSeasonTurn::Even(All);
	size_t Next = 0;
	for (FTile& Tile : Tiles)
	{
		Tile.First = 1.0f;
		Tile.Last = 0.0f;
		for (float& A : Tile.Arrive)
		{
			A = All[Next++];
			Tile.Note(A);
		}
	}
	Took = FPlatformTime::Seconds() - Start;
}

namespace
{
	/** The ground round the rules' mud copied out for the other thread: nothing it is given is the map's own. */
	TSharedPtr<FSeasonTurnTable::FMudWork, ESPMode::ThreadSafe> SeasonMudWork(const FCampaignMapData& Data, const FCampaignTerrain& Terrain, const FCampaignGround& Paint, const TArray<TArray<FVector2D>>& RiverPaths, const TArray<int32>& Regions)
	{
		TArray<bool> InMud;
		InMud.Init(false, Data.Regions.Num());
		FBox2D Reach(ForceInit);
		for (const int32 Id : Regions)
		{
			if (Data.Regions.IsValidIndex(Id))
			{
				InMud[Id] = true;
				if (Data.Regions[Id].Bounds.bIsValid)
				{
					Reach += Data.Regions[Id].Bounds;
				}
			}
		}
		if (!Reach.bIsValid || Terrain.NX <= 0 || Terrain.NY <= 0)
		{
			return nullptr;
		}
		// (and some way beyond, so that the water just outside the mud's ground is known to it)
		Reach = Reach.ExpandBy(120.0);
		const int32 I0 = FMath::Clamp(FMath::FloorToInt32((Reach.Min.X - Terrain.OriginX) / Terrain.Cell), 0, Terrain.NX - 1);
		const int32 I1 = FMath::Clamp(FMath::CeilToInt32((Reach.Max.X - Terrain.OriginX) / Terrain.Cell), I0, Terrain.NX - 1);
		const int32 J0 = FMath::Clamp(FMath::FloorToInt32((Reach.Min.Y - Terrain.OriginY) / Terrain.Cell), 0, Terrain.NY - 1);
		const int32 J1 = FMath::Clamp(FMath::CeilToInt32((Reach.Max.Y - Terrain.OriginY) / Terrain.Cell), J0, Terrain.NY - 1);
		TSharedPtr<FSeasonTurnTable::FMudWork, ESPMode::ThreadSafe> Work = MakeShared<FSeasonTurnTable::FMudWork, ESPMode::ThreadSafe>();
		FSeasonTurnTable::FMudWork& W = *Work;
		W.Wide = Terrain.NX;
		W.High = Terrain.NY;
		W.I0 = I0;
		W.J0 = J0;
		W.NX = I1 - I0 + 1;
		W.NY = J1 - J0 + 1;
		W.Cell = Terrain.Cell;
		W.OriginX = Terrain.OriginX;
		W.OriginY = Terrain.OriginY;
		const int32 Count = W.NX * W.NY;
		W.Kind.SetNumUninitialized(Count);
		W.Height.SetNumUninitialized(Count);
		W.Rough.SetNumUninitialized(Count);
		W.Ground.SetNumUninitialized(Count);
		const bool bRough = Terrain.Roughness.Num() == Terrain.Height.Num();
		ParallelFor(W.NY, [&](int32 J)
		{
			for (int32 I = 0; I < W.NX; ++I)
			{
				const int32 Idx = Terrain.Index(I0 + I, J0 + J), At = J * W.NX + I;
				const int32 Region = Terrain.Region[Idx];
				W.Kind[At] = static_cast<uint8>(Terrain.Coverage[Idx] < 0.5f ? 2 : (Region >= 0 && InMud.IsValidIndex(Region) && InMud[Region]) ? 1 : 0);
				W.Height[At] = Terrain.Height[Idx];
				W.Rough[At] = bRough ? Terrain.Roughness[Idx] : 0.0f;
				W.Ground[At] = Paint.Ground[Idx];
			}
		});
		for (const TArray<FVector2D>& Path : RiverPaths)
		{
			for (const FVector2D& Point : Path)
			{
				if (Reach.IsInside(Point))
				{
					W.Waters.Add(Path);
					break;
				}
			}
		}
		for (const FCampaignLake& Lake : Data.Lakes)
		{
			if (Lake.Outline.Num() > 0 && Reach.Intersect(FBox2D(Lake.Outline)))
			{
				TArray<FVector2D>& Shore = W.Waters.Add_GetRef(Lake.Outline);
				Shore.Add(Lake.Outline[0]);
			}
		}
		return Work;
	}

	/** Paints the squares of a picture a front is crossing (or has crossed since it was last painted), and sends just those (through Send: the box and how many of its smaller copies, so that churned ground is laid over them as they go). */
	template <typename FSendBox, typename FPaintCell>
	void PaintSeasonPlay(FSeasonTurnTable::FPlay& Play, TArray<FSeasonTurnTable::FTile>& Tiles, double FrontNow, FSendBox&& Send, FPaintCell&& PaintCell)
	{
		// (a point changes while the front stands between its arrival and its arrival and the band behind it)
		const double Changing = FMath::Max(TheaterSeasonTurn::FrontWidth, TheaterSeasonTurn::BandWidth);
		TArray<int32> Active;
		for (int32 T = 0; T < Tiles.Num(); ++T)
		{
			const FSeasonTurnTable::FTile& Tile = Tiles[T];
			if (Tile.Cells.Num() == 0)
			{
				continue;
			}
			const double First = Play.bBackward ? 1.0 - Tile.Last : Tile.First;
			const double Last = Play.bBackward ? 1.0 - Tile.First : Tile.Last;
			if (FrontNow > First && Play.Painted < Last + Changing)
			{
				Active.Add(T);
			}
		}
		ParallelFor(Active.Num(), [&](int32 K)
		{
			FSeasonTurnTable::FTile& Tile = Tiles[Active[K]];
			for (int32 C = 0; C < Tile.Cells.Num(); ++C)
			{
				PaintCell(Tile, C);
			}
		});
		// Sent a run of squares at a time along each row of them, with their first three smaller copies (the rest
		// when it has settled: a far view sees the turn a moment late, never wrongly).
		for (int32 K = 0; K < Active.Num();)
		{
			int32 End = K;
			while (End + 1 < Active.Num() && Active[End + 1] == Active[End] + 1 && Tiles[Active[End + 1]].Y0 == Tiles[Active[K]].Y0)
			{
				++End;
			}
			const FSeasonTurnTable::FTile& From = Tiles[Active[K]];
			Send(From.X0, From.Y0, Tiles[Active[End]].X1, From.Y1, 3);
			K = End + 1;
		}
		Play.Painted = FrontNow;
	}
}

const TArray<FColor>& ACampaignMapActor::SeasonGroundShown() const
{
	// (release 2, churned ground: what the ground picture holds now, under the churn)
	if (SeasonTurn.IsValid() && SeasonTurn->Ground.Num() == Paint.Ground.Num() && (SeasonTurn->MudLies() || SeasonTurn->Mud.bOn))
	{
		return SeasonTurn->Ground;
	}
	return Paint.Ground;
}

void ACampaignMapActor::SeasonMasksShown(TArray<FColor>& Out) const
{
	// (and the masks: the season's own while its snow is spreading over them; the month before's while the new snow
	// waits for its turn; else the month's snow as SetSeason sent it)
	if (SeasonTurn.IsValid() && SeasonTurn->Snow.bOn && SeasonTurn->Masks.Num() == Paint.Masks.Num())
	{
		Out = SeasonTurn->Masks;
		return;
	}
	if (SeasonTurn.IsValid() && SeasonTurn->bSnowTaken)
	{
		MakeSnowMasks(SeasonTurn->HardBefore, Out);
		return;
	}
	MakeSnowMasks(MasksSentFor >= 0.0f ? MasksSentFor : WinterHard, Out);
}

void ACampaignMapActor::NoteSeason(int32 Month, float Hard, float HardBefore, bool bStepped)
{
	// (a few numbers, kept whether the switch is on or not, so that a turn switched on in play knows the season)
	if (!SeasonTurn.IsValid())
	{
		SeasonTurn = MakeShared<FSeasonTurnTable, ESPMode::ThreadSafe>();
		SeasonTurn->bWasOn = SeasonTurnWanted();
	}
	FSeasonTurnTable& Table = *SeasonTurn;
	Table.Month = Month;
	Table.Hard = Hard;
	Table.HardBefore = HardBefore;
	Table.bStepNow = bStepped;
	if (bStepped)
	{
		Table.StepAt = FPlatformTime::Seconds();
	}
}

bool ACampaignMapActor::TakeSeasonSnow(TArray<FColor>& Pixels)
{
	if (!SeasonTurn.IsValid() || !SeasonTurnWanted())
	{
		return false;
	}
	FSeasonTurnTable& Table = *SeasonTurn;
	const bool bStep = Table.bStepNow;
	Table.bStepNow = false;
	// A turn still under way when the next month comes (the months going by quickly) ends there, and the new month's
	// snow is laid at once.
	const bool bWasTurning = Table.bSnowTaken || Table.Snow.bOn;
	Table.SettleSnow();
	Table.Masks.Empty();
	Table.bSendMasks = false;
	if (!bStep || bWasTurning || CVarSeasonTurnPlay.GetValueOnGameThread() == 0 || Pixels.Num() != Terrain.NX * Terrain.NY)
	{
		return false;
	}
	// The old month's snow stays on the table until the turn spreads the new over it.
	Table.Masks = MoveTemp(Pixels);
	Table.bSnowTaken = true;
	Table.SnowTakenAt = FPlatformTime::Seconds();
	return true;
}

bool ACampaignMapActor::AskSeasonMud(const TArray<int32>& Regions)
{
	if (!SeasonTurnWanted())
	{
		return false;
	}
	if (!SeasonTurn.IsValid())
	{
		SeasonTurn = MakeShared<FSeasonTurnTable, ESPMode::ThreadSafe>();
		SeasonTurn->bWasOn = true;
	}
	FSeasonTurnTable& Table = *SeasonTurn;
	// (asked again while the last was still being worked out or spreading: what was spreading ends at once, and the
	// new is laid at once)
	Table.bMudRush = Table.Mud.bOn || Table.MudWork.IsValid();
	Table.SettleMud(Paint.Ground);
	Table.MudWork.Reset();
	Table.MudAsked = Regions;
	Table.bMudAsk = true;
	Table.MudAskedAt = FPlatformTime::Seconds();
	++Table.MudGen;
	return true;
}

void ACampaignMapActor::UpdateSeasonTurn()
{
	if (!SeasonTurn.IsValid())
	{
		return;
	}
	FSeasonTurnTable& Table = *SeasonTurn;
	const bool bOn = SeasonTurnWanted();
	if (bOn != Table.bWasOn)
	{
		// Switched in play: the mud is laid, or taken up and the wash laid again, at once; snow under way ends.
		Table.bWasOn = bOn;
		if (!bOn)
		{
			if (Table.Snow.bOn || Table.bSnowTaken || Table.bSendMasks)
			{
				Table.SettleSnow();
				SendMasks(Table.Masks);
			}
			Table.Masks.Empty();
			Table.bSendMasks = false;
			const bool bGroundChanged = Table.Ground.Num() > 0;
			Table.MudWork.Reset();
			++Table.MudGen;
			Table.bMudAsk = false;
			Table.Mud = FSeasonTurnTable::FPlay();
			Table.MudTiles.Empty();
			Table.Ground.Empty();
			Table.bSendGround = false;
			if (bGroundChanged)
			{
				SendGround(Paint.Ground);
			}
		}
		const TArray<int32> Was = ShownMud;
		if (Was.Num() > 0)
		{
			ShownMud.Reset();
			ShowMud(Was);
		}
		UE_LOG(LogTheater, Log, TEXT("Season turn: switched %s in play"), bOn ? TEXT("on") : TEXT("off"));
	}
	if (!bOn)
	{
		return;
	}
	const double Now = FPlatformTime::Seconds();
	const float Replay = CVarSeasonTurnReplay.GetValueOnGameThread();
	const bool bReplay = Replay > 0.0f && Replay != Table.ReplayPlayed;
	if (!Table.Busy() && !bReplay)
	{
		return;
	}
	THEATER_SPEED_SCOPE(SeasonTurnFrame);
	if (!GroundPicture || !MasksPicture || Paint.Ground.Num() != Terrain.NX * Terrain.NY || Paint.Masks.Num() != Paint.Ground.Num() || Terrain.Region.Num() != Paint.Ground.Num())
	{
		return; // (the table is not laid yet)
	}

	// ---- (tc.Map.SeasonTurnReplay) this month's turn again, from the month before, for pictures ----
	if (bReplay)
	{
		THEATER_SPEED_SCOPE(SeasonTurnReplay);
		Table.ReplayPlayed = Replay;
		Table.StepAt = Now;
		const bool bSnowUnsent = Table.Snow.bOn || Table.bSnowTaken || Table.bSendMasks;
		Table.SettleSnow();
		if (bSnowUnsent && Table.Hard == Table.HardBefore)
		{
			SendMasks(Table.Masks);
		}
		Table.Masks.Empty();
		Table.bSendMasks = false;
		if (Table.Hard != Table.HardBefore)
		{
			TArray<FColor> Before;
			MakeSnowMasks(Table.HardBefore, Before);
			SendMasks(Before);
			MakeSnowMasks(Table.Hard, Table.Masks);
			Table.bSnowTaken = true;
			Table.SnowTakenAt = Now;
		}
		Table.SettleMud(Paint.Ground);
		if (Table.MudLies() && Table.Ground.Num() == Paint.Ground.Num())
		{
			for (const FSeasonTurnTable::FTile& Tile : Table.MudTiles)
			{
				for (const int32 Cell : Tile.Cells)
				{
					Table.Ground[Cell] = Paint.Ground[Cell];
				}
			}
			SendGround(Table.Ground);
			Table.bSendGround = false;
			Table.Begin(Table.Mud, Now, false);
		}
		UE_LOG(LogTheater, Log, TEXT("Season turn: month %d played again from the month before (snow %s, mud %s)"), Table.Month + 1,
			Table.bSnowTaken ? TEXT("changes") : TEXT("as it was"), Table.Mud.bOn ? TEXT("soaks in") : TEXT("none to soak in"));
	}

	// ---- the mud asked for: copied out in a frame of its own, a fifth of a second after the ask, and worked out on another thread ----
	if (Table.bMudAsk && Now - Table.MudAskedAt >= 0.2)
	{
		Table.bMudAsk = false;
		// A turn if the month stepped on by one in play about when the mud was asked for (ShowMud and SetSeason come in
		// either order in the same update).
		const bool bTurn = CVarSeasonTurnPlay.GetValueOnGameThread() != 0 && !Table.bMudRush && FMath::Abs(Table.MudAskedAt - Table.StepAt) < 3.0;
		Table.bMudRush = false;
		if (Table.MudAsked.Num() == 0)
		{
			if (Table.MudLies())
			{
				if (bTurn)
				{
					Table.Begin(Table.Mud, Now, true); // it dries from its far edges back to the water
				}
				else
				{
					Table.LiftMud(Paint.Ground);
				}
			}
		}
		else
		{
			THEATER_SPEED_SCOPE(SeasonMudCopy);
			TSharedPtr<FSeasonTurnTable::FMudWork, ESPMode::ThreadSafe> Work = SeasonMudWork(Data, Terrain, Paint, RiverPaths, Table.MudAsked);
			if (Work.IsValid())
			{
				Work->Gen = Table.MudGen;
				Work->bTurn = bTurn;
				Work->Much = FMath::Clamp(CVarSeasonTurnMuch.GetValueOnGameThread(), 0.0f, 1.5f);
				Table.MudWork = Work;
				Table.MudBuilding = Async(EAsyncExecution::ThreadPool, [Work]()
				{
					Work->Work();
					Work->bDone.store(true, std::memory_order_release);
				});
			}
			else
			{
				Table.LiftMud(Paint.Ground);
			}
		}
	}
	// ---- the mud worked out: taken in, spreading or laid at once ----
	if (Table.MudWork.IsValid() && Table.MudWork->bDone.load(std::memory_order_acquire))
	{
		const TSharedPtr<FSeasonTurnTable::FMudWork, ESPMode::ThreadSafe> Work = Table.MudWork;
		Table.MudWork.Reset();
		if (Work->Gen == Table.MudGen)
		{
			if (Table.Ground.Num() != Paint.Ground.Num())
			{
				Table.Ground = Paint.Ground;
			}
			// (mud that lay before is taken up and the whole picture sent before any of the new spreads; so is a turn
			// that was brought to its end when this was asked for)
			const bool bWasUnsent = Table.bSendGround, bHadMud = Table.MudLies();
			Table.LiftMud(Paint.Ground);
			Table.bSendGround = bWasUnsent || bHadMud;
			Table.MudTiles = MoveTemp(Work->Tiles);
			if (Work->bTurn && !bHadMud)
			{
				Table.Begin(Table.Mud, Now, false);
			}
			else
			{
				for (const FSeasonTurnTable::FTile& Tile : Table.MudTiles)
				{
					for (int32 C = 0; C < Tile.Cells.Num(); ++C)
					{
						Table.Ground[Tile.Cells[C]] = Tile.Whole[C];
					}
				}
				Table.bSendGround = true;
			}
			UE_LOG(LogTheater, Log, TEXT("Season turn: mud on %d points of %d provinces, worked out in %.0f ms on another thread (%s)"), Work->Muddy, Table.MudAsked.Num(),
				Work->Took * 1000.0, Table.Mud.bOn ? TEXT("soaking in") : TEXT("laid at once"));
		}
	}
	// ---- the new snow: its squares laid in a frame of its own, a tenth of a second after it was handed over ----
	if (Table.bSnowTaken && Now - Table.SnowTakenAt >= 0.1)
	{
		THEATER_SPEED_SCOPE(SeasonSnowLay);
		Table.bSnowTaken = false;
		const int32 Wide = Terrain.NX, High = Terrain.NY;
		TArray<FColor> Before;
		MakeSnowMasks(Table.HardBefore, Before);
		if (Before.Num() == Table.Masks.Num() && Table.Masks.Num() == Wide * High)
		{
			FSeasonTurnTable::MakeTiles(Wide, High, Table.SnowTiles);
			TArray<FSeasonTurnTable::FTile>& Tiles = Table.SnowTiles;
			ParallelFor(Tiles.Num(), [&](int32 T)
			{
				FSeasonTurnTable::FTile& Tile = Tiles[T];
				for (int32 J = Tile.Y0; J <= Tile.Y1; ++J)
				{
					for (int32 I = Tile.X0; I <= Tile.X1; ++I)
					{
						const int32 Idx = Terrain.Index(I, J);
						if (Before[Idx].G != Table.Masks[Idx].G)
						{
							Tile.Cells.Add(Idx);
							Tile.SnowWas.Add(Before[Idx].G);
							Tile.SnowWill.Add(Table.Masks[Idx].G);
						}
					}
				}
			});
			// The north and the south of the ground whose snow is changing.
			int32 North = High, South = -1;
			for (const FSeasonTurnTable::FTile& Tile : Tiles)
			{
				if (Tile.Cells.Num() > 0)
				{
					North = FMath::Min(North, Tile.Cells[0] / Wide);
					South = FMath::Max(South, Tile.Cells.Last() / Wide);
				}
			}
			const double Across = FMath::Max(1, South - North);
			// The heights a square of four points at a time (high ground comes first, not each crest of the table's tooth).
			constexpr int32 Coarse = 4;
			const int32 CoarseWide = FMath::DivideAndRoundUp(Wide, Coarse), CoarseHigh = FMath::DivideAndRoundUp(High, Coarse);
			TArray<float> Lie;
			Lie.SetNumUninitialized(CoarseWide * CoarseHigh);
			ParallelFor(CoarseHigh, [&](int32 CJ)
			{
				for (int32 CI = 0; CI < CoarseWide; ++CI)
				{
					double Sum = 0.0;
					int32 Points = 0;
					for (int32 J = CJ * Coarse; J < FMath::Min(High, (CJ + 1) * Coarse); ++J)
					{
						for (int32 I = CI * Coarse; I < FMath::Min(Wide, (CI + 1) * Coarse); ++I)
						{
							Sum += Terrain.Height[Terrain.Index(I, J)];
							++Points;
						}
					}
					Lie[CJ * CoarseWide + CI] = static_cast<float>(Sum / FMath::Max(1, Points));
				}
			});
			auto LieAt = [&](int32 I, int32 J)
			{
				const double X = FMath::Clamp((I + 0.5) / Coarse - 0.5, 0.0, CoarseWide - 1.0), Y = FMath::Clamp((J + 0.5) / Coarse - 0.5, 0.0, CoarseHigh - 1.0);
				const int32 X0 = FMath::Min(static_cast<int32>(X), CoarseWide - 1), Y0 = FMath::Min(static_cast<int32>(Y), CoarseHigh - 1);
				const int32 X1 = FMath::Min(X0 + 1, CoarseWide - 1), Y1 = FMath::Min(Y0 + 1, CoarseHigh - 1);
				const double FX = X - X0, FY = Y - Y0;
				return FMath::Lerp(FMath::Lerp(Lie[Y0 * CoarseWide + X0], Lie[Y0 * CoarseWide + X1], FX), FMath::Lerp(Lie[Y1 * CoarseWide + X0], Lie[Y1 * CoarseWide + X1], FX), FY);
			};
			ParallelFor(Tiles.Num(), [&](int32 T)
			{
				FSeasonTurnTable::FTile& Tile = Tiles[T];
				Tile.Arrive.SetNumUninitialized(Tile.Cells.Num());
				for (int32 C = 0; C < Tile.Cells.Num(); ++C)
				{
					const int32 Idx = Tile.Cells[C], I = Idx % Wide, J = Idx / Wide;
					const FVector2D P = Terrain.PointAt(I, J);
					const double South01 = (J - North) / Across;
					const double High01 = FMath::Clamp((LieAt(I, J) - 1.0) / 4.0, 0.0, 1.0);
					const double Grain = 0.65 * TheaterSeasonTurn::Noise(P.X / 16.0, P.Y / 16.0, 91u) + 0.35 * TheaterSeasonTurn::Noise(P.X / 5.0, P.Y / 5.0, 93u);
					Tile.Arrive[C] = TheaterSeasonTurn::SnowArrive(South01, High01, Grain, Tile.SnowWill[C] > Tile.SnowWas[C]);
					Table.Masks[Idx].G = Tile.SnowWas[C]; // (as it lies now, until the front reaches it)
				}
			});
			// The arrivals spread evenly over the turn, as the mud's are.
			{
				std::vector<float> All;
				for (const FSeasonTurnTable::FTile& Tile : Tiles)
				{
					All.insert(All.end(), Tile.Arrive.GetData(), Tile.Arrive.GetData() + Tile.Arrive.Num());
				}
				TheaterSeasonTurn::Even(All);
				size_t Next = 0;
				for (FSeasonTurnTable::FTile& Tile : Tiles)
				{
					for (float& A : Tile.Arrive)
					{
						A = All[Next++];
						Tile.Note(A);
					}
				}
			}
			int32 Changing = 0;
			for (const FSeasonTurnTable::FTile& Tile : Tiles)
			{
				Changing += Tile.Cells.Num();
			}
			if (Changing > 0)
			{
				Table.Begin(Table.Snow, Now, false);
			}
			else
			{
				Table.SnowTiles.Empty();
				Table.Masks.Empty();
			}
			UE_LOG(LogTheater, Log, TEXT("Season turn: the snow %s on %d points"), Table.Hard > Table.HardBefore ? TEXT("comes") : TEXT("goes"), Changing);
		}
		else
		{
			// (the table changed under it: the new month's snow at once)
			SendMasks(Table.Masks);
			Table.Masks.Empty();
		}
	}

	// ---- a key or click settles every turn under way in a fifth of a second ----
	const double Held = CVarSeasonTurnAt.GetValueOnGameThread() >= 0.0f ? static_cast<double>(CVarSeasonTurnAt.GetValueOnGameThread()) : -1.0;
	if (Held < 0.0 && ((Table.Snow.bOn && Now >= Table.Snow.Began) || (Table.Mud.bOn && Now >= Table.Mud.Began)) && SeasonTurnKeyPressed(GetWorld()))
	{
		Table.SkippedAt = Now;
		for (FSeasonTurnTable::FPlay* Play : { &Table.Snow, &Table.Mud })
		{
			if (Play->bOn && Play->SkipAt < 0.0)
			{
				Play->SkipFrom = Play->FrontAt(Now, -1.0);
				Play->SkipAt = Now;
			}
		}
	}

	// ---- a frame of the turn: at most thirty a second, and always its last ----
	bool bPainted = false;
	if (Table.Snow.bOn || Table.Mud.bOn)
	{
		const double SnowFront = Table.Snow.bOn ? Table.Snow.FrontAt(Now, Held) : 0.0;
		const double MudFront = Table.Mud.bOn ? Table.Mud.FrontAt(Now, Held) : 0.0;
		const bool bEnding = (Table.Snow.bOn && TheaterSeasonTurn::IsSettled(SnowFront)) || (Table.Mud.bOn && TheaterSeasonTurn::IsSettled(MudFront));
		if (bEnding || Now - Table.LastPaint >= 1.0 / 30.0 - 0.002)
		{
			THEATER_SPEED_SCOPE(SeasonTurnPaint);
			if (Table.Snow.bOn && SnowFront > Table.Snow.Painted)
			{
				bPainted = true;
				PaintSeasonPlay(Table.Snow, Table.SnowTiles, SnowFront, [&](int32 X0, int32 Y0, int32 X1, int32 Y1, int32 MaxLevel) { SendMasksBox(Table.Masks, X0, Y0, X1, Y1, MaxLevel); }, [&](FSeasonTurnTable::FTile& Tile, int32 C)
				{
					Table.Masks[Tile.Cells[C]].G = TheaterSeasonTurn::SnowNow(Tile.SnowWas[C], Tile.SnowWill[C], TheaterSeasonTurn::At(Tile.Arrive[C], SnowFront));
				});
				if (TheaterSeasonTurn::IsSettled(SnowFront))
				{
					Table.Snow = FSeasonTurnTable::FPlay();
					Table.SnowTiles.Empty();
					Table.bSendMasks = true;
					UE_LOG(LogTheater, Log, TEXT("Season turn: the snow has settled"));
				}
			}
			if (Table.Mud.bOn && MudFront > Table.Mud.Painted && Table.Ground.Num() == Paint.Ground.Num())
			{
				bPainted = true;
				const bool bDrying = Table.Mud.bBackward;
				const TArray<FColor>& DryGround = Paint.Ground;
				PaintSeasonPlay(Table.Mud, Table.MudTiles, MudFront, [&](int32 X0, int32 Y0, int32 X1, int32 Y1, int32 MaxLevel) { SendGroundBox(Table.Ground, X0, Y0, X1, Y1, MaxLevel); }, [&](FSeasonTurnTable::FTile& Tile, int32 C)
				{
					const int32 Cell = Tile.Cells[C];
					const FColor Dry = DryGround[Cell], Whole = Tile.Whole[C], Banded = Tile.Banded[C];
					TheaterSeasonTurn::FShown Shown;
					if (bDrying)
					{
						Shown.Share = 1.0f - TheaterSeasonTurn::At(1.0f - Tile.Arrive[C], MudFront).Share;
					}
					else
					{
						Shown = TheaterSeasonTurn::At(Tile.Arrive[C], MudFront);
					}
					const uint8 From[3] = { Dry.R, Dry.G, Dry.B }, ToWhole[3] = { Whole.R, Whole.G, Whole.B }, ToBanded[3] = { Banded.R, Banded.G, Banded.B };
					uint8 Out[3];
					TheaterSeasonTurn::MixMud(From, ToWhole, ToBanded, Shown, Out);
					Table.Ground[Cell] = FColor(Out[0], Out[1], Out[2], Dry.A);
				});
				if (TheaterSeasonTurn::IsSettled(MudFront))
				{
					Table.Mud = FSeasonTurnTable::FPlay();
					if (bDrying)
					{
						Table.MudTiles.Empty();
					}
					Table.bSendGround = true;
					UE_LOG(LogTheater, Log, TEXT("Season turn: the mud has %s"), bDrying ? TEXT("dried") : TEXT("soaked in"));
				}
			}
			if (bPainted)
			{
				Table.LastPaint = Now;
			}
		}
	}

	// ---- the whole picture, once a turn has settled (its smaller copies with it): one picture a frame, never in a frame that painted ----
	if (!bPainted && (Table.bSendMasks || Table.bSendGround))
	{
		THEATER_SPEED_SCOPE(SeasonTurnSend);
		if (Table.bSendMasks)
		{
			Table.bSendMasks = false;
			if (Table.Masks.Num() == Paint.Masks.Num())
			{
				SendMasks(Table.Masks);
			}
			Table.Masks.Empty();
		}
		else
		{
			Table.bSendGround = false;
			if (Table.MudLies() || Table.Mud.bOn)
			{
				SendGround(Table.Ground);
			}
			else
			{
				SendGround(Paint.Ground);
				Table.Ground.Empty();
			}
		}
	}
}
