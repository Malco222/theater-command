#include "Campaign/CampaignMapActor.h"

#include "Campaign/CampaignMiniatures.h"
#include "Campaign/CampaignTextures.h"
#include "Campaign/FrontChurn.h"
#include "Campaign/FrontLine.h"
#include "Campaign/TheaterSpeed.h"
#include "Campaign/TheaterStillMesh.h"
#include "TheaterCommand.h"
#include "Async/Async.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "ProceduralMeshComponent.h"

#include <atomic>
#include <cmath>
#include <vector>

/*
 * (Release 2, the campaign map look builder: "fronts that feel alive", piece 1, churned ground between dug-in lines.
 * The art director's sheet art-director/fronts-alive/fronts-alive-look.md of 27 November 2026 and her read of the first
 * cloud drawings, churn-prototype-read-1127.md, which rules where the two differ; the arithmetic, and its cloud checks
 * and drawings, are in Campaign/FrontChurn.h.) The table's side of it:
 *
 *  - Which fronts. Each view the controller shows (ShowFrontChurn) gives the fronts as pairs of provinces and the corps
 *    on them. A pair has churned ground between it where both sides stand dug in: for as many months as the fewer of
 *    them where both are known; where only one side's months are known (the enemy seen digging, his months hidden by
 *    the fog: the design director's ruling of 27 November, 19:36Z), for its months; where neither is, for the months
 *    this table has seen them so. Nothing the fog hides is read: a corps only glimpsed, or known from a report, says
 *    no more than its posture. When the front moves on or a side leaves, the ground heals where it lies, over a year.
 *    (A campaign taken up again shows no healing ground from before it was saved until the rules record it: the known
 *    limit, with the design director's rules row as its mend.)
 *  - The lines. Laid in a frame of their own after a view that changed them, never in the month's own frame: the
 *    border of every such pair, chained, thinned and rounded as the pen's front is (Campaign/FrontLine.h), each point
 *    given its pair's state and how far it lies from where that pair's armies face, the pairs joined at the stronger
 *    and the open ends broken up.
 *  - The ground picture. The band's own earth (its hue and darkness, the woods thinned and the snow broken in its core)
 *    is laid into the ground picture and the masks every time either is sent, by whoever sends it (the table's paint,
 *    the season turn's mud and snow), and never kept in their arrays: this table keeps the pictures as sent, and lays
 *    and lifts only its own points. Each of the picture's squares (128 a side) shows it as far as the band is wide on
 *    the screen there: it fades out where the band would be under about four pixels, where the pen's ink carries the
 *    front (the art director's read). A square that changes is sent with its first four smaller copies, two a frame.
 *  - The fine veil and the craters, near the eye only, in squares of 24 map units: the veil (the crisp ragged edge and
 *    its tongues, the grain, the clusters as darker mottle) is worked out on another thread and laid as a see-through
 *    mesh that only darkens, right by day and by night; the craters and flecks of spoil are modelled and lit by the
 *    table's own light, drawn from three pixels across and whole at six, at most tc.Map.FrontChurnCraters of them in
 *    view, nearest the eye first.
 *  - The woods. The trees standing close up go where the band's core has thinned the woods in the picture.
 *
 * Off (tc.Map.FrontChurn 0, as it comes), nothing here runs: every send of the pictures is the plain send it was, and
 * no table is made. Turned off in play, the ground is given back at once and the meshes are put away.
 */

namespace
{
	TAutoConsoleVariable<int32> CVarFrontChurn(
		TEXT("tc.Map.FrontChurn"), 0,
		TEXT("1: where both sides of a front have stood dug in and facing for months, the ground between them is churned into a band of broken, cratered earth along it (from the second month, whole at the sixth), healing over a year once the front moves on; 0: none, as before. Release 2; off as it comes."));
	TAutoConsoleVariable<float> CVarFrontChurnMonths(
		TEXT("tc.Map.FrontChurnMonths"), -1.0f,
		TEXT("With tc.Map.FrontChurn, for pictures: 0 or more takes every front on the table as dug in on both sides for this many months (6 is whole); -1 reads the corps, as the game has it."));
	TAutoConsoleVariable<float> CVarFrontChurnHeal(
		TEXT("tc.Map.FrontChurnHeal"), -1.0f,
		TEXT("With tc.Map.FrontChurn, for pictures: 0 or more takes every front on the table as left this many months ago, after standing tc.Map.FrontChurnMonths (or six), and healing; -1 as the game has it."));
	TAutoConsoleVariable<int32> CVarFrontChurnCraters(
		TEXT("tc.Map.FrontChurnCraters"), 600,
		TEXT("With tc.Map.FrontChurn: the most craters modelled in view at once, nearest the eye first (0 to 3000)."));

	bool FrontChurnWanted()
	{
		static const bool bAsked = FParse::Param(FCommandLine::Get(), TEXT("TheaterFrontChurn"));
		return bAsked || CVarFrontChurn.GetValueOnGameThread() != 0;
	}

	/** A pair of provinces, the lower number first. */
	uint64 ChurnPairKey(int32 One, int32 Other)
	{
		const uint64 Low = static_cast<uint32>(FMath::Min(One, Other)), High = static_cast<uint32>(FMath::Max(One, Other));
		return (Low << 32) | High;
	}

	constexpr int32 ChurnPictureSquare = 128; // the ground picture's squares, as the season turn sends them
	constexpr int32 ChurnPictureLevels = 4; // a square is sent with this many of its smaller copies (a band of four units is lost in the rest)
	constexpr int32 ChurnSendsAFrame = 2; // squares of the pictures sent a frame
	constexpr double ChurnVeilSquare = 24.0; // map units: the fine veil's and the craters' squares
	constexpr double ChurnVeilStep = 0.15; // the veil's lattice
	constexpr double ChurnVeilLift = 0.1; // over the ground: under the frontier's own line (0.13) and the pen (0.15, 0.2)
	constexpr double ChurnBandWide = 3.6; // the whole band, core and edges, for how many pixels wide it is on the screen
	constexpr int32 ChurnWorkersAtOnce = 2; // veil squares worked out on other threads at once

	/** The largest of two states: ground still healing from an earlier stand keeps what is left of it where that is more. */
	TheaterChurn::FState ChurnLarger(const TheaterChurn::FState& One, const TheaterChurn::FState& Other)
	{
		TheaterChurn::FState Out;
		Out.Size = FMath::Max(One.Size, Other.Size);
		Out.Dark = FMath::Max(One.Dark, Other.Dark);
		Out.Pits = FMath::Max(One.Pits, Other.Pits);
		Out.Wide = FMath::Max(One.Wide, Other.Wide);
		Out.Pale = One.Size >= Other.Size ? One.Pale : Other.Pale;
		return Out;
	}

	uint8 ChurnMixByte(uint8 From, uint8 To, float Share)
	{
		return static_cast<uint8>(FMath::Clamp(FMath::RoundToInt32(static_cast<float>(From) + (static_cast<float>(To) - static_cast<float>(From)) * Share), 0, 255));
	}

	FColor ChurnMix(const FColor& From, const FColor& To, float Share)
	{
		if (Share <= 0.0f)
		{
			return From;
		}
		if (Share >= 1.0f)
		{
			return To;
		}
		return FColor(ChurnMixByte(From.R, To.R, Share), ChurnMixByte(From.G, To.G, Share), ChurnMixByte(From.B, To.B, Share), ChurnMixByte(From.A, To.A, Share));
	}

	/** The nearest point of a stretch to a point. */
	FVector2D ChurnNearestOn(const FVector2D& Point, const FVector2D& From, const FVector2D& To)
	{
		const FVector2D Way = To - From;
		const double Long2 = Way.X * Way.X + Way.Y * Way.Y;
		const double Share = Long2 > 1.0e-12 ? FMath::Clamp(((Point.X - From.X) * Way.X + (Point.Y - From.Y) * Way.Y) / Long2, 0.0, 1.0) : 0.0;
		return From + Way * Share;
	}
}

/** A point of the ground picture with churned ground on it. */
struct FFrontChurnCell
{
	int32 Index = 0; // in the ground picture (and the masks)
	int32 Square = 0; // its square of the pictures
	int32 Region = INDEX_NONE;
	double X = 0.0, Y = 0.0;
	TheaterChurn::FBand Band;
	TheaterChurn::FState State;
	TheaterChurn::EGround Ground = TheaterChurn::EGround::Common;
	FColor In; // the ground as the picture has it without the churn
	FColor Out; // the churned ground worked out for OutFor in OutSeason
	FColor OutFor;
	uint8 OutSeason = 255;
	uint8 SnowIn = 0; // the snow the masks have there without it
	uint8 SnowOut = 0; // and the broken snow worked out for SnowFor
	uint8 SnowFor = 0;
	uint8 Woods = 0; // the woods left standing in the core, for the trees close up (0 to 255)
};

/** A square of the fine veil, worked out on another thread: what it is given (copies: it never touches the map), and what it gives back. */
struct FFrontChurnVeilWork
{
	TSharedPtr<const std::vector<TheaterChurn::FLine>, ESPMode::ThreadSafe> Lines;
	int32 Gen = 0;
	double OriginX = 0.0, OriginY = 0.0;
	int32 Across = 0;
	// (what it must not lie on: how much land there is at each of the terrain's points round the square, and the towns near it)
	double LandOriginX = 0.0, LandOriginY = 0.0, LandCell = 1.5;
	int32 LandNX = 0, LandNY = 0;
	TArray<float> Land;
	TArray<FVector> Towns; // X, Y and radius, map units
	TArray<uint8> Alpha; // (back) Across by Across: how much the veil darkens each point
	int32 Lit = 0;
	std::atomic<bool> bDone{ false };
	void Work();
	float LandShare(double X, double Y) const;
};

/** A square of the fine veil and the craters (24 map units a side). */
struct FFrontChurnTile
{
	int32 Mesh = INDEX_NONE; // its mesh among the map's ChurnMeshes, while it has one
	bool bVeil = false; // its veil is laid on the mesh, for the lines as they stand
	TSharedPtr<FFrontChurnVeilWork, ESPMode::ThreadSafe> Work;
	TFuture<void> Building;
	TArray<TheaterChurn::FPit> Pits;
	int32 CratersFor = INDEX_NONE; // the half-octave of the screen's pixels its craters were made for (none: none made)
	int32 Craters = 0; // how many it has modelled
	float Shown = -1.0f; // how far its veil is faded in
	double Far = 0.0; // from the eye, in the world
	double PixelsAUnit = 0.0;
	double AwaySince = -1.0;
};

/** What the churned ground keeps between frames (ACampaignMapActor::FrontChurn): made by the first view with the switch on. */
struct FFrontChurnTable
{
	int32 Turn = INDEX_NONE;

	// ---- the pairs of provinces on the fronts, as the views have shown them ----
	struct FPair
	{
		int32 SeenFrom = INDEX_NONE; // the month both sides were first seen dug in, where neither side's months are known (an in-play count)
		bool bActive = false; // both sides dug in and facing, as the last view showed
		TheaterChurn::FState Now; // as it stands
		TheaterChurn::FState AtLeaving; // (healing) as it stood when the front moved on or a side left
		int32 LeftTurn = INDEX_NONE; // and when that was
	};
	TMap<uint64, FPair> Pairs;
	FString LaidFor; // the pairs and their states (in twentieths) the lines were last laid for
	bool bLayDue = false;
	int32 Gen = 0; // each laying of the lines

	// ---- the lines, and the ground picture's points they touch ----
	TSharedPtr<const std::vector<TheaterChurn::FLine>, ESPMode::ThreadSafe> Lines;
	TArray<FFrontChurnCell> Cells;
	TMap<int32, int32> CellAt; // the picture's point -> its place in Cells
	TArray<FColor> ShownGround, ShownMasks; // the pictures as sent, churned ground and all, while the switch is on
	int32 SquaresAcross = 1;
	TMap<int32, uint8> SquareShown; // each square holding churned ground: how much of it shows, in eighths
	TSet<int32> GroundDue, MasksDue; // squares to be sent again
	TSet<int32> MudSet; // the rules' mud ground, as the cells were last laid for
	uint32 MudFor = 0;

	// ---- the fine veil and the craters near the eye ----
	TMap<FIntPoint, FFrontChurnTile> Tiles;
	TArray<int32> SpareMeshes;
	int32 Working = 0; // squares being worked out on other threads
	TSet<FIntPoint> TreeSquares; // the woods' squares (130 units a side) the churned ground lies in, to be planted again when it changes

	/** A square's work, if any, let go (its thread finishes into nothing: it holds its own copies). */
	void Drop(FFrontChurnTile& Tile)
	{
		if (Tile.Work.IsValid())
		{
			Tile.Work.Reset();
			Working = FMath::Max(0, Working - 1);
		}
	}
};

void FFrontChurnVeilWork::Work()
{
	Alpha.SetNumZeroed(Across * Across);
	if (!Lines.IsValid() || Across <= 0)
	{
		return;
	}
	const std::vector<TheaterChurn::FLine>& Laid = *Lines;
	TheaterChurn::FNearest Nearest;
	TheaterChurn::Measure(Laid, OriginX, OriginY, ChurnVeilStep, Across, Across, Nearest);
	for (int32 J = 0; J < Across; ++J)
	{
		for (int32 I = 0; I < Across; ++I)
		{
			const int32 Point = J * Across + I;
			if (Nearest.Line[static_cast<size_t>(Point)] < 0)
			{
				continue;
			}
			const double X = OriginX + I * ChurnVeilStep, Y = OriginY + J * ChurnVeilStep;
			TheaterChurn::FBandIn In;
			if (!TheaterChurn::At(Laid, Nearest, Point, X, Y, In))
			{
				continue;
			}
			const TheaterChurn::FBand Band = TheaterChurn::Band(In);
			double Dark = TheaterChurn::Veil(Band, In.State, X, Y);
			if (Dark <= 0.0)
			{
				continue;
			}
			// (none at sea; none in the towns, with a soft half unit round them)
			Dark *= FMath::SmoothStep(0.45, 0.6, static_cast<double>(LandShare(X, Y)));
			for (const FVector& Town : Towns)
			{
				const double Off = FMath::Sqrt((X - Town.X) * (X - Town.X) + (Y - Town.Y) * (Y - Town.Y));
				Dark *= FMath::SmoothStep(Town.Z, Town.Z + 0.5, Off);
			}
			const int32 Byte = FMath::Clamp(FMath::RoundToInt32(Dark * 255.0), 0, 255);
			Alpha[Point] = static_cast<uint8>(Byte);
			Lit += Byte > 0 ? 1 : 0;
		}
	}
}

float FFrontChurnVeilWork::LandShare(double X, double Y) const
{
	if (LandNX < 2 || LandNY < 2)
	{
		return 1.0f;
	}
	const double U = FMath::Clamp((X - LandOriginX) / LandCell, 0.0, static_cast<double>(LandNX - 1) - 1.0e-6);
	const double V = FMath::Clamp((Y - LandOriginY) / LandCell, 0.0, static_cast<double>(LandNY - 1) - 1.0e-6);
	const int32 I = FMath::FloorToInt32(U), J = FMath::FloorToInt32(V);
	const int32 I1 = FMath::Min(I + 1, LandNX - 1), J1 = FMath::Min(J + 1, LandNY - 1);
	const float A = static_cast<float>(U - I), B = static_cast<float>(V - J);
	const float Top = FMath::Lerp(Land[J * LandNX + I], Land[J * LandNX + I1], A);
	const float Bottom = FMath::Lerp(Land[J1 * LandNX + I], Land[J1 * LandNX + I1], A);
	return FMath::Lerp(Top, Bottom, B);
}

// ---- the views -------------------------------------------------------------------------------------------------------

void ACampaignMapActor::ShowFrontChurn(const FCampaignView& View)
{
	if (!FrontChurnWanted() || !Data.IsLoaded())
	{
		return;
	}
	THEATER_SPEED_SCOPE(ShowFrontChurn);
	if (!FrontChurn.IsValid())
	{
		FrontChurn = MakeShared<FFrontChurnTable, ESPMode::ThreadSafe>();
	}
	FFrontChurnTable& Table = *FrontChurn;
	Table.Turn = View.Turn;

	// Each province with corps dug in on it: for how many months, where the view says (0 where it does not).
	struct FDug
	{
		int32 Months = 0;
		bool bUnknown = false;
	};
	TMap<int32, FDug> DugIn;
	for (const FViewCorps& Corps : View.Corps)
	{
		if (!Data.Regions.IsValidIndex(Corps.Region))
		{
			continue;
		}
		const bool bKnown = !Corps.bDim && Corps.Stale == 0;
		if (bKnown && Corps.bDug)
		{
			FDug& Here = DugIn.FindOrAdd(Corps.Region);
			if (Corps.DugFor > 0)
			{
				Here.Months = FMath::Max(Here.Months, Corps.DugFor);
			}
			else
			{
				Here.bUnknown = true; // (dug in where months are not counted)
			}
		}
		else if (!bKnown && Corps.Posture == TEXT("dig"))
		{
			DugIn.FindOrAdd(Corps.Region).bUnknown = true; // (glimpsed, or from a report: its posture and no more)
		}
	}

	const float AskedMonths = CVarFrontChurnMonths.GetValueOnGameThread();
	const float AskedHeal = CVarFrontChurnHeal.GetValueOnGameThread();
	auto Stand = [](FFrontChurnTable::FPair& Pair, double Months, int32 Turn)
	{
		if (Months >= 0.0)
		{
			TheaterChurn::FState Grown = TheaterChurn::Growing(Months);
			if (Pair.LeftTurn != INDEX_NONE)
			{
				const TheaterChurn::FState Left = TheaterChurn::Healing(Pair.AtLeaving, static_cast<double>(Turn - Pair.LeftTurn));
				if (TheaterChurn::IsNothing(Left) || Left.Size <= Grown.Size)
				{
					Pair.LeftTurn = INDEX_NONE;
				}
				else
				{
					Grown = ChurnLarger(Grown, Left);
				}
			}
			Pair.bActive = true;
			Pair.Now = Grown;
			return;
		}
		Pair.SeenFrom = INDEX_NONE;
		if (Pair.bActive)
		{
			Pair.bActive = false;
			if (!TheaterChurn::IsNothing(Pair.Now))
			{
				Pair.AtLeaving = Pair.Now;
				Pair.LeftTurn = Turn;
			}
		}
		Pair.Now = Pair.LeftTurn != INDEX_NONE ? TheaterChurn::Healing(Pair.AtLeaving, static_cast<double>(Turn - Pair.LeftTurn)) : TheaterChurn::FState();
	};
	TSet<uint64> OnFront;
	for (int32 K = 0; K + 1 < View.Fronts.Num(); K += 2)
	{
		const int32 One = View.Fronts[K], Other = View.Fronts[K + 1];
		if (!Data.Regions.IsValidIndex(One) || !Data.Regions.IsValidIndex(Other) || One == Other)
		{
			continue;
		}
		const uint64 Key = ChurnPairKey(One, Other);
		OnFront.Add(Key);
		FFrontChurnTable::FPair& Pair = Table.Pairs.FindOrAdd(Key);
		if (AskedMonths >= 0.0f || AskedHeal >= 0.0f)
		{
			// (for pictures: every front as asked, nothing of the game's own kept)
			const TheaterChurn::FState Stood = TheaterChurn::Growing(AskedMonths >= 0.0f ? AskedMonths : 6.0);
			Pair = FFrontChurnTable::FPair();
			Pair.Now = AskedHeal >= 0.0f ? TheaterChurn::Healing(Stood, AskedHeal) : Stood;
			continue;
		}
		double Months = -1.0; // dug in and facing for so many months; below nought, not both dug in
		const FDug* Here = DugIn.Find(One);
		const FDug* There = DugIn.Find(Other);
		if (Here && There)
		{
			if (Here->Months > 0 && There->Months > 0)
			{
				Months = FMath::Min(Here->Months, There->Months);
			}
			else if (Here->Months > 0 || There->Months > 0)
			{
				Months = FMath::Max(Here->Months, There->Months); // (the other seen digging, its months unknown: the known side's)
			}
			else
			{
				if (Pair.SeenFrom == INDEX_NONE)
				{
					Pair.SeenFrom = View.Turn;
				}
				Months = static_cast<double>(View.Turn - Pair.SeenFrom + 1);
			}
		}
		Stand(Pair, Months, View.Turn);
	}
	// Fronts no longer on the table: their ground heals where it lies; gone a year on, it is forgotten.
	TArray<uint64> Gone;
	for (TPair<uint64, FFrontChurnTable::FPair>& Each : Table.Pairs)
	{
		if (!OnFront.Contains(Each.Key))
		{
			Stand(Each.Value, -1.0, View.Turn);
		}
		if (!Each.Value.bActive && Each.Value.SeenFrom == INDEX_NONE && TheaterChurn::IsNothing(Each.Value.Now))
		{
			Gone.Add(Each.Key);
		}
	}
	for (const uint64 Key : Gone)
	{
		Table.Pairs.Remove(Key);
	}

	// Laid again (a frame later) only where what shows has changed.
	TArray<uint64> Keys;
	Table.Pairs.GetKeys(Keys);
	Keys.Sort();
	FString Showing;
	auto Twentieths = [](float Share) { return FMath::RoundToInt32(Share * 20.0f); };
	for (const uint64 Key : Keys)
	{
		const TheaterChurn::FState& Now = Table.Pairs.FindChecked(Key).Now;
		if (!TheaterChurn::IsNothing(Now))
		{
			Showing += FString::Printf(TEXT("%llu:%d,%d,%d,%d,%d;"), static_cast<unsigned long long>(Key), Twentieths(Now.Size), Twentieths(Now.Dark), Twentieths(Now.Pits), Twentieths(Now.Pale), Twentieths(Now.Wide));
		}
	}
	if (Showing != Table.LaidFor)
	{
		Table.LaidFor = Showing;
		Table.bLayDue = true;
	}
}

// ---- the lines ---------------------------------------------------------------------------------------------------------

void ACampaignMapActor::LayFrontChurn()
{
	THEATER_SPEED_SCOPE(LayFrontChurn);
	FFrontChurnTable& Table = *FrontChurn;
	++Table.Gen;
	const double Started = FPlatformTime::Seconds();
	FindSharedEdges();

	// The border of every pair with churned ground on it.
	TMap<uint64, TheaterChurn::FState> Lying;
	for (const TPair<uint64, FFrontChurnTable::FPair>& Each : Table.Pairs)
	{
		if (!TheaterChurn::IsNothing(Each.Value.Now))
		{
			Lying.Add(Each.Key, Each.Value.Now);
		}
	}
	std::vector<TheaterFront::FFrontPiece> ChurnPieces;
	TArray<uint64> PieceKeys;
	for (const FSharedEdge& Shared : SharedEdges)
	{
		const uint64 Key = ChurnPairKey(Shared.Here, Shared.There);
		if (Shared.Here < 0 || Shared.There < 0 || !Lying.Contains(Key))
		{
			continue;
		}
		TheaterFront::FFrontPiece& Made = ChurnPieces.emplace_back();
		Made.A = TheaterFront::FPoint{ Shared.A.X, Shared.A.Y };
		Made.B = TheaterFront::FPoint{ Shared.B.X, Shared.B.Y };
		Made.Low = FMath::Min(Shared.Here, Shared.There);
		Made.High = FMath::Max(Shared.Here, Shared.There);
		PieceKeys.Add(Key);
	}

	// Chained, thinned and rounded as the pen's front is, and divided every half unit.
	TheaterFront::FRules ChainRules;
	ChainRules.Step = 0.5;
	TheaterFront::FScratch Scratch;
	std::vector<TheaterFront::FChain> Chains;
	TheaterFront::ChainFront(ChurnPieces, ChainRules, Scratch, Chains);
	for (TheaterFront::FChain& Chain : Chains)
	{
		TheaterFront::Despike(Chain);
		TheaterFront::Thin(Chain, ChainRules.Thin, Scratch);
		TheaterFront::Round(Chain, ChainRules.Passes, ChainRules.Cut, Scratch);
	}
	TheaterFront::MeetLines(Chains);
	for (TheaterFront::FChain& Chain : Chains)
	{
		TheaterFront::Divide(Chain, ChainRules.Step, Scratch);
	}

	// Where each pair's armies face: the point of their shared border nearest the middle between their towns. And the
	// pieces in squares of four units, for each point of the lines to find its own pair.
	TMap<uint64, FVector2D> FaceAt;
	TMap<FIntPoint, TArray<int32>> PieceSquares;
	const double PieceSquare = 4.0;
	for (int32 P = 0; P < PieceKeys.Num(); ++P)
	{
		const TheaterFront::FFrontPiece& Piece = ChurnPieces[static_cast<size_t>(P)];
		const FVector2D From(Piece.A.X, Piece.A.Y), To(Piece.B.X, Piece.B.Y);
		const FVector2D Middle = (Data.Regions[Piece.Low].Center + Data.Regions[Piece.High].Center) * 0.5;
		const FVector2D Facing = ChurnNearestOn(Middle, From, To);
		const FVector2D* Best = FaceAt.Find(PieceKeys[P]);
		if (!Best || FVector2D::DistSquared(Facing, Middle) < FVector2D::DistSquared(*Best, Middle))
		{
			FaceAt.Add(PieceKeys[P], Facing);
		}
		const int32 S0 = FMath::FloorToInt32(FMath::Min(From.X, To.X) / PieceSquare), S1 = FMath::FloorToInt32(FMath::Max(From.X, To.X) / PieceSquare);
		const int32 T0 = FMath::FloorToInt32(FMath::Min(From.Y, To.Y) / PieceSquare), T1 = FMath::FloorToInt32(FMath::Max(From.Y, To.Y) / PieceSquare);
		for (int32 T = T0; T <= T1; ++T)
		{
			for (int32 S = S0; S <= S1; ++S)
			{
				PieceSquares.FindOrAdd(FIntPoint(S, T)).Add(P);
			}
		}
	}

	TSharedPtr<std::vector<TheaterChurn::FLine>, ESPMode::ThreadSafe> Made = MakeShared<std::vector<TheaterChurn::FLine>, ESPMode::ThreadSafe>();
	std::vector<TheaterChurn::FLine>& Laid = *Made;
	for (const TheaterFront::FChain& Chain : Chains)
	{
		if (Chain.Points.size() < 2)
		{
			continue;
		}
		TheaterChurn::FLine& Line = Laid.emplace_back();
		Line.bClosed = Chain.bClosed;
		for (const TheaterFront::FPoint& Point : Chain.Points)
		{
			const FVector2D At(Point.X, Point.Y);
			int32 Best = INDEX_NONE;
			double BestFar = 1.0e30;
			const int32 S = FMath::FloorToInt32(At.X / PieceSquare), T = FMath::FloorToInt32(At.Y / PieceSquare);
			for (int32 DT = -1; DT <= 1; ++DT)
			{
				for (int32 DS = -1; DS <= 1; ++DS)
				{
					if (const TArray<int32>* Near = PieceSquares.Find(FIntPoint(S + DS, T + DT)))
					{
						for (const int32 P : *Near)
						{
							const TheaterFront::FFrontPiece& Piece = ChurnPieces[static_cast<size_t>(P)];
							const double Far = FVector2D::DistSquared(At, ChurnNearestOn(At, FVector2D(Piece.A.X, Piece.A.Y), FVector2D(Piece.B.X, Piece.B.Y)));
							if (Far < BestFar)
							{
								BestFar = Far;
								Best = P;
							}
						}
					}
				}
			}
			const uint64 Key = Best != INDEX_NONE ? PieceKeys[Best] : 0;
			const TheaterChurn::FState* State = Lying.Find(Key);
			const FVector2D* Facing = FaceAt.Find(Key);
			Line.X.push_back(At.X);
			Line.Y.push_back(At.Y);
			Line.Facing.push_back(Facing ? static_cast<float>(FVector2D::Distance(At, *Facing)) : 1.0e6f);
			Line.State.push_back(State ? *State : TheaterChurn::FState());
		}
		TheaterChurn::Strengthen(Line);
		if (!Line.bClosed)
		{
			TheaterChurn::EaseEnds(Line);
		}
	}
	Table.Lines = Made;

	// ---- the ground picture's points the lines touch ----
	const int32 GridNX = Terrain.NX, GridNY = Terrain.NY;
	TMap<int32, double> TownKept; // (each province's town's reach, worked out once)
	auto TownReach = [this, &TownKept](int32 RegionId) -> double
	{
		if (const double* Kept = TownKept.Find(RegionId))
		{
			return *Kept;
		}
		return TownKept.Add(RegionId, TownRadius(RegionId));
	};
	Table.SquaresAcross = FMath::Max(1, FMath::DivideAndRoundUp(GridNX, ChurnPictureSquare));
	TArray<FFrontChurnCell> Fresh;
	TMap<int32, int32> FreshAt;
	TSet<FIntPoint> FreshTiles;
	if (Laid.size() > 0)
	{
		double MinX = 1.0e30, MinY = 1.0e30, MaxX = -1.0e30, MaxY = -1.0e30;
		for (const TheaterChurn::FLine& Line : Laid)
		{
			for (size_t K = 0; K < Line.X.size(); ++K)
			{
				MinX = FMath::Min(MinX, Line.X[K]);
				MaxX = FMath::Max(MaxX, Line.X[K]);
				MinY = FMath::Min(MinY, Line.Y[K]);
				MaxY = FMath::Max(MaxY, Line.Y[K]);
				// (and the veil's squares within reach of it)
				const int32 S0 = FMath::FloorToInt32((Line.X[K] - TheaterChurn::Touch) / ChurnVeilSquare), S1 = FMath::FloorToInt32((Line.X[K] + TheaterChurn::Touch) / ChurnVeilSquare);
				const int32 T0 = FMath::FloorToInt32((Line.Y[K] - TheaterChurn::Touch) / ChurnVeilSquare), T1 = FMath::FloorToInt32((Line.Y[K] + TheaterChurn::Touch) / ChurnVeilSquare);
				for (int32 T = T0; T <= T1; ++T)
				{
					for (int32 S = S0; S <= S1; ++S)
					{
						FreshTiles.Add(FIntPoint(S, T));
					}
				}
			}
		}
		const int32 I0 = FMath::Clamp(FMath::FloorToInt32((MinX - TheaterChurn::Touch - Terrain.OriginX) / Terrain.Cell), 0, GridNX - 1);
		const int32 I1 = FMath::Clamp(FMath::CeilToInt32((MaxX + TheaterChurn::Touch - Terrain.OriginX) / Terrain.Cell), 0, GridNX - 1);
		const int32 J0 = FMath::Clamp(FMath::FloorToInt32((MinY - TheaterChurn::Touch - Terrain.OriginY) / Terrain.Cell), 0, GridNY - 1);
		const int32 J1 = FMath::Clamp(FMath::CeilToInt32((MaxY + TheaterChurn::Touch - Terrain.OriginY) / Terrain.Cell), 0, GridNY - 1);
		const int32 BoxNX = I1 - I0 + 1, BoxNY = J1 - J0 + 1;
		TheaterChurn::FNearest Nearest;
		TheaterChurn::Measure(Laid, Terrain.OriginX + I0 * Terrain.Cell, Terrain.OriginY + J0 * Terrain.Cell, Terrain.Cell, BoxNX, BoxNY, Nearest);
		for (int32 BJ = 0; BJ < BoxNY; ++BJ)
		{
			for (int32 BI = 0; BI < BoxNX; ++BI)
			{
				const int32 Local = BJ * BoxNX + BI;
				if (Nearest.Line[static_cast<size_t>(Local)] < 0)
				{
					continue;
				}
				const int32 GI = I0 + BI, GJ = J0 + BJ, Index = Terrain.Index(GI, GJ);
				const FVector2D Place = Terrain.PointAt(GI, GJ);
				const int32 RegionId = Terrain.Region[Index];
				if (RegionId < 0 || !Data.Regions.IsValidIndex(RegionId) || Terrain.Coverage[Index] < 0.5f)
				{
					continue; // (the sea)
				}
				if (FVector2D::Distance(Place, Data.Regions[RegionId].Center) < TownReach(RegionId) + 1.0)
				{
					continue; // (a town)
				}
				TheaterChurn::FBandIn In;
				if (!TheaterChurn::At(Laid, Nearest, Local, Place.X, Place.Y, In))
				{
					continue;
				}
				const TheaterChurn::FBand Band = TheaterChurn::Band(In);
				if (Band.Body < 0.004f && Band.Core < 0.004f)
				{
					continue;
				}
				FFrontChurnCell& Cell = Fresh.AddDefaulted_GetRef();
				Cell.Index = Index;
				Cell.Square = (GI / ChurnPictureSquare) + (GJ / ChurnPictureSquare) * Table.SquaresAcross;
				Cell.Region = RegionId;
				Cell.X = Place.X;
				Cell.Y = Place.Y;
				Cell.Band = Band;
				Cell.State = In.State;
				// The ground it lies on: the desert, the black earth of the steppe (its plains and hills from about 46 to 53
				// north and 27 to 50 east), or the rest of Europe.
				const FString& Kind = Data.Regions[RegionId].Terrain;
				const double Lat = Data.LatitudeAt(Place.Y), Lon = Data.LongitudeAt(Place.X);
				Cell.Ground = Kind == TEXT("desert") ? TheaterChurn::EGround::Desert
					: (Kind == TEXT("plains") || Kind == TEXT("hills")) && Lat >= 46.0 && Lat <= 53.0 && Lon >= 27.0 && Lon <= 50.0 ? TheaterChurn::EGround::BlackEarth
					: TheaterChurn::EGround::Common;
				Cell.Woods = TheaterChurn::Wooded(Paint.Ground[Index].A, Band, In.State);
				FreshAt.Add(Index, Fresh.Num() - 1);
			}
		}
	}

	// ---- the cells swapped in: what is no longer churned is given back, what is new is laid (each square as it shows) ----
	const bool bKept = Table.ShownGround.Num() == Paint.Ground.Num() && Table.ShownMasks.Num() == Paint.Masks.Num();
	TSet<int32> Changed;
	for (const FFrontChurnCell& Old : Table.Cells)
	{
		if (int32* Now = FreshAt.Find(Old.Index))
		{
			Fresh[*Now].In = Old.In;
			Fresh[*Now].SnowIn = Old.SnowIn;
		}
		else if (bKept)
		{
			Table.ShownGround[Old.Index] = Old.In;
			Table.ShownMasks[Old.Index].G = Old.SnowIn;
		}
		Changed.Add(Old.Square);
	}
	for (FFrontChurnCell& Cell : Fresh)
	{
		if (!Table.CellAt.Contains(Cell.Index) && bKept)
		{
			Cell.In = Table.ShownGround[Cell.Index];
			Cell.SnowIn = Table.ShownMasks[Cell.Index].G;
		}
		Changed.Add(Cell.Square);
	}
	Table.Cells = MoveTemp(Fresh);
	Table.CellAt = MoveTemp(FreshAt);
	TMap<int32, uint8> WasShown = MoveTemp(Table.SquareShown);
	Table.SquareShown.Reset();
	for (const FFrontChurnCell& Cell : Table.Cells)
	{
		const uint8* Was = WasShown.Find(Cell.Square);
		Table.SquareShown.Add(Cell.Square, Was ? *Was : static_cast<uint8>(8));
	}
	if (bKept)
	{
		for (FFrontChurnCell& Cell : Table.Cells)
		{
			LayChurnCell(Cell);
		}
	}
	for (const int32 Square : Changed)
	{
		Table.GroundDue.Add(Square);
		Table.MasksDue.Add(Square);
	}

	// ---- the craters, into the veil's squares; squares no longer near the lines put away, the rest laid again ----
	TMap<FIntPoint, TArray<TheaterChurn::FPit>> PitsIn;
	{
		std::vector<TheaterChurn::FPit> All;
		for (const TheaterChurn::FLine& Line : Laid)
		{
			All.clear();
			TheaterChurn::Pits(Line, All);
			for (const TheaterChurn::FPit& Pit : All)
			{
				const int32 GI = FMath::Clamp(FMath::RoundToInt32((Pit.X - Terrain.OriginX) / Terrain.Cell), 0, GridNX - 1);
				const int32 GJ = FMath::Clamp(FMath::RoundToInt32((Pit.Y - Terrain.OriginY) / Terrain.Cell), 0, GridNY - 1);
				const int32 Index = Terrain.Index(GI, GJ);
				const int32 RegionId = Terrain.Region[Index];
				if (RegionId < 0 || !Data.Regions.IsValidIndex(RegionId) || Terrain.CoverageAt(Pit.X, Pit.Y) < 0.6f
					|| FVector2D::Distance(FVector2D(Pit.X, Pit.Y), Data.Regions[RegionId].Center) < TownReach(RegionId) + 1.0)
				{
					continue;
				}
				PitsIn.FindOrAdd(FIntPoint(FMath::FloorToInt32(Pit.X / ChurnVeilSquare), FMath::FloorToInt32(Pit.Y / ChurnVeilSquare))).Add(Pit);
			}
		}
	}
	TArray<FIntPoint> Away;
	for (TPair<FIntPoint, FFrontChurnTile>& Each : Table.Tiles)
	{
		if (!FreshTiles.Contains(Each.Key))
		{
			Away.Add(Each.Key);
		}
	}
	for (const FIntPoint& Key : Away)
	{
		PutChurnTileAway(Table.Tiles.FindChecked(Key));
		Table.Tiles.Remove(Key);
	}
	for (const FIntPoint& Key : FreshTiles)
	{
		FFrontChurnTile& Tile = Table.Tiles.FindOrAdd(Key);
		Tile.bVeil = false;
		Table.Drop(Tile);
		Tile.CratersFor = INDEX_NONE;
		TArray<TheaterChurn::FPit>* Pits = PitsIn.Find(Key);
		Tile.Pits = Pits ? MoveTemp(*Pits) : TArray<TheaterChurn::FPit>();
	}

	// ---- the trees close up: the woods' squares the churned ground lay in, or lies in now, planted again ----
	TSet<FIntPoint> TreesNow;
	for (const FFrontChurnCell& Cell : Table.Cells)
	{
		if (Cell.Woods != Paint.Ground[Cell.Index].A)
		{
			TreesNow.Add(FIntPoint(FMath::FloorToInt32(Cell.X / 130.0), FMath::FloorToInt32(Cell.Y / 130.0)));
		}
	}
	TSet<FIntPoint> TreesAgain = TreesNow.Union(Table.TreeSquares);
	Table.TreeSquares = MoveTemp(TreesNow);
	for (int32 Mesh = 0; Mesh < TreeSquareOf.Num(); ++Mesh)
	{
		if (TreesAgain.Contains(TreeSquareOf[Mesh]))
		{
			TreesDue.AddUnique(Mesh);
		}
	}

	int32 Points = 0;
	for (const TheaterChurn::FLine& Line : Laid)
	{
		Points += static_cast<int32>(Line.X.size());
	}
	UE_LOG(LogTheater, Log, TEXT("Churned ground (tc.Map.FrontChurn): %d pairs dug in or healing, %d lines (%d points) from %d stretches of border, %d points of the ground picture, %d squares of veil; laid in %.1f ms"),
		Lying.Num(), static_cast<int32>(Laid.size()), Points, static_cast<int32>(ChurnPieces.size()), Table.Cells.Num(), Table.Tiles.Num(), (FPlatformTime::Seconds() - Started) * 1000.0);
}

// ---- the ground picture and the masks ------------------------------------------------------------------------------

void ACampaignMapActor::LayChurnCell(FFrontChurnCell& Cell)
{
	FFrontChurnTable& Table = *FrontChurn;
	const uint8* Eighths = Table.SquareShown.Find(Cell.Square);
	const float Share = Eighths ? static_cast<float>(*Eighths) / 8.0f : 1.0f;
	// The season on it: the rules' mud, snow lying (as the masks have it there), or dry.
	const TheaterChurn::ESeason Season = Table.MudSet.Contains(Cell.Region) ? TheaterChurn::ESeason::Mud
		: Cell.SnowIn > 77 ? TheaterChurn::ESeason::Snow : TheaterChurn::ESeason::Dry;
	if (Cell.OutSeason != static_cast<uint8>(Season) || !(Cell.OutFor == Cell.In))
	{
		const uint8 From[3] = { Cell.In.R, Cell.In.G, Cell.In.B };
		uint8 To[3] = { Cell.In.R, Cell.In.G, Cell.In.B };
		TheaterChurn::PaintCell(From, Cell.Band, Cell.State, Cell.Ground, Season, Cell.X, Cell.Y, To);
		Cell.Out = FColor(To[0], To[1], To[2], TheaterChurn::Wooded(Cell.In.A, Cell.Band, Cell.State));
		Cell.OutFor = Cell.In;
		Cell.OutSeason = static_cast<uint8>(Season);
	}
	if (Cell.SnowFor != Cell.SnowIn)
	{
		Cell.SnowOut = TheaterChurn::Snow(Cell.SnowIn, Cell.Band, Cell.State);
		Cell.SnowFor = Cell.SnowIn;
	}
	if (Table.ShownGround.IsValidIndex(Cell.Index))
	{
		Table.ShownGround[Cell.Index] = ChurnMix(Cell.In, Cell.Out, Share);
	}
	if (Table.ShownMasks.IsValidIndex(Cell.Index))
	{
		Table.ShownMasks[Cell.Index].G = ChurnMixByte(Cell.SnowIn, Cell.SnowOut, Share);
	}
}

void ACampaignMapActor::SendGround(const TArray<FColor>& Pixels)
{
	if (!FrontChurn.IsValid() || Pixels.Num() != Paint.Ground.Num())
	{
		CampaignTextures::Update(GroundPicture, Pixels);
		return;
	}
	THEATER_SPEED_SCOPE(ChurnSendGround);
	FFrontChurnTable& Table = *FrontChurn;
	Table.ShownGround = Pixels;
	for (FFrontChurnCell& Cell : Table.Cells)
	{
		Cell.In = Pixels[Cell.Index];
		LayChurnCell(Cell);
	}
	Table.GroundDue.Reset();
	CampaignTextures::Update(GroundPicture, Table.ShownGround);
}

void ACampaignMapActor::SendGroundBox(const TArray<FColor>& Pixels, int32 X0, int32 Y0, int32 X1, int32 Y1, int32 MaxLevel)
{
	if (!FrontChurn.IsValid() || Pixels.Num() != Paint.Ground.Num() || FrontChurn->ShownGround.Num() != Pixels.Num())
	{
		CampaignTextures::UpdateBoxAll(GroundPicture, Pixels, X0, Y0, X1, Y1, MaxLevel);
		if (FrontChurn.IsValid() && Pixels.Num() == Paint.Ground.Num())
		{
			FrontChurn->ShownGround = Pixels; // (kept from now on)
		}
		return;
	}
	FFrontChurnTable& Table = *FrontChurn;
	const int32 Wide = Terrain.NX;
	X0 = FMath::Clamp(X0, 0, Wide - 1);
	X1 = FMath::Clamp(X1, X0, Wide - 1);
	Y0 = FMath::Clamp(Y0, 0, Terrain.NY - 1);
	Y1 = FMath::Clamp(Y1, Y0, Terrain.NY - 1);
	for (int32 Row = Y0; Row <= Y1; ++Row)
	{
		FMemory::Memcpy(Table.ShownGround.GetData() + Row * Wide + X0, Pixels.GetData() + Row * Wide + X0, (X1 - X0 + 1) * sizeof(FColor));
	}
	for (FFrontChurnCell& Cell : Table.Cells)
	{
		const int32 I = Cell.Index % Wide, J = Cell.Index / Wide;
		if (I >= X0 && I <= X1 && J >= Y0 && J <= Y1)
		{
			Cell.In = Pixels[Cell.Index];
			LayChurnCell(Cell);
		}
	}
	CampaignTextures::UpdateBoxAll(GroundPicture, Table.ShownGround, X0, Y0, X1, Y1, MaxLevel);
}

void ACampaignMapActor::SendMasks(const TArray<FColor>& Pixels)
{
	if (!FrontChurn.IsValid() || Pixels.Num() != Paint.Masks.Num())
	{
		CampaignTextures::Update(MasksPicture, Pixels);
		return;
	}
	THEATER_SPEED_SCOPE(ChurnSendMasks);
	FFrontChurnTable& Table = *FrontChurn;
	Table.ShownMasks = Pixels;
	for (FFrontChurnCell& Cell : Table.Cells)
	{
		const bool bWasSnow = Cell.SnowIn > 77;
		Cell.SnowIn = Pixels[Cell.Index].G;
		LayChurnCell(Cell);
		if (bWasSnow != (Cell.SnowIn > 77))
		{
			Table.GroundDue.Add(Cell.Square); // (the snow came or went under it: its earth is painted for the season)
		}
	}
	Table.MasksDue.Reset();
	CampaignTextures::Update(MasksPicture, Table.ShownMasks);
}

void ACampaignMapActor::SendMasksBox(const TArray<FColor>& Pixels, int32 X0, int32 Y0, int32 X1, int32 Y1, int32 MaxLevel)
{
	if (!FrontChurn.IsValid() || Pixels.Num() != Paint.Masks.Num() || FrontChurn->ShownMasks.Num() != Pixels.Num())
	{
		CampaignTextures::UpdateBoxAll(MasksPicture, Pixels, X0, Y0, X1, Y1, MaxLevel);
		if (FrontChurn.IsValid() && Pixels.Num() == Paint.Masks.Num())
		{
			FrontChurn->ShownMasks = Pixels;
		}
		return;
	}
	FFrontChurnTable& Table = *FrontChurn;
	const int32 Wide = Terrain.NX;
	X0 = FMath::Clamp(X0, 0, Wide - 1);
	X1 = FMath::Clamp(X1, X0, Wide - 1);
	Y0 = FMath::Clamp(Y0, 0, Terrain.NY - 1);
	Y1 = FMath::Clamp(Y1, Y0, Terrain.NY - 1);
	for (int32 Row = Y0; Row <= Y1; ++Row)
	{
		FMemory::Memcpy(Table.ShownMasks.GetData() + Row * Wide + X0, Pixels.GetData() + Row * Wide + X0, (X1 - X0 + 1) * sizeof(FColor));
	}
	for (FFrontChurnCell& Cell : Table.Cells)
	{
		const int32 I = Cell.Index % Wide, J = Cell.Index / Wide;
		if (I >= X0 && I <= X1 && J >= Y0 && J <= Y1)
		{
			const bool bWasSnow = Cell.SnowIn > 77;
			Cell.SnowIn = Pixels[Cell.Index].G;
			LayChurnCell(Cell);
			if (bWasSnow != (Cell.SnowIn > 77))
			{
				Table.GroundDue.Add(Cell.Square);
			}
		}
	}
	CampaignTextures::UpdateBoxAll(MasksPicture, Table.ShownMasks, X0, Y0, X1, Y1, MaxLevel);
}

float ACampaignMapActor::ChurnWoods(int32 Index, float Wooded) const
{
	if (!FrontChurn.IsValid() || FrontChurn->Cells.Num() == 0)
	{
		return Wooded;
	}
	const int32* At = FrontChurn->CellAt.Find(Index);
	return At ? FMath::Min(Wooded, static_cast<float>(FrontChurn->Cells[*At].Woods) / 255.0f) : Wooded;
}

// ---- the fine veil and the craters ---------------------------------------------------------------------------------

void ACampaignMapActor::PutChurnTileAway(FFrontChurnTile& Tile)
{
	if (Tile.Mesh != INDEX_NONE && ChurnMeshes.IsValidIndex(Tile.Mesh) && ChurnMeshes[Tile.Mesh])
	{
		ChurnMeshes[Tile.Mesh]->ClearAllMeshSections();
		ChurnMeshes[Tile.Mesh]->SetVisibility(false);
		FrontChurn->SpareMeshes.Add(Tile.Mesh);
	}
	Tile.Mesh = INDEX_NONE;
	Tile.bVeil = false;
	Tile.CratersFor = INDEX_NONE;
	Tile.Craters = 0;
	Tile.Shown = -1.0f;
	FrontChurn->Drop(Tile);
}

void ACampaignMapActor::BuildChurnVeil(FFrontChurnTile& Tile, const FFrontChurnVeilWork& Work)
{
	THEATER_SPEED_SCOPE(ChurnVeil);
	UProceduralMeshComponent* Mesh = ChurnMeshes.IsValidIndex(Tile.Mesh) ? ChurnMeshes[Tile.Mesh].Get() : nullptr;
	if (!Mesh)
	{
		return;
	}
	const int32 Across = Work.Across;
	// (the way round that faces up, as the table's own meshes wind it (FCampaignMeshBuffers::AddTriangleFacingUp), the
	// same for every square on the flat: the map's x runs east, its y south)
	const FVector Corner0 = MapToWorld(0.0, 0.0), Corner1 = MapToWorld(1.0, 0.0), Corner2 = MapToWorld(1.0, 1.0);
	const bool bUpAsLaid = ((Corner1 - Corner2) ^ (Corner0 - Corner2)).Z >= 0.0;
	TArray<int32> Made;
	Made.Init(INDEX_NONE, Across * Across);
	TArray<FVector> Vertices, Normals;
	TArray<int32> Triangles;
	TArray<FColor> Colors;
	Vertices.Reserve(Work.Lit * 2);
	auto VeilPoint = [&](int32 I, int32 J) -> int32
	{
		int32& Point = Made[J * Across + I];
		if (Point == INDEX_NONE)
		{
			Point = Vertices.Add(SurfacePoint(Work.OriginX + I * ChurnVeilStep, Work.OriginY + J * ChurnVeilStep, ChurnVeilLift));
			Normals.Add(FVector::UpVector);
			Colors.Add(FColor(0, 0, 0, Work.Alpha[J * Across + I]));
		}
		return Point;
	};
	for (int32 J = 0; J + 1 < Across; ++J)
	{
		for (int32 I = 0; I + 1 < Across; ++I)
		{
			const uint8 A = Work.Alpha[J * Across + I], B = Work.Alpha[J * Across + I + 1];
			const uint8 C = Work.Alpha[(J + 1) * Across + I + 1], D = Work.Alpha[(J + 1) * Across + I];
			if ((A | B | C | D) == 0)
			{
				continue;
			}
			const int32 P00 = VeilPoint(I, J), P10 = VeilPoint(I + 1, J), P11 = VeilPoint(I + 1, J + 1), P01 = VeilPoint(I, J + 1);
			if (bUpAsLaid)
			{
				Triangles.Append({ P00, P10, P11, P00, P11, P01 });
			}
			else
			{
				Triangles.Append({ P00, P11, P10, P00, P01, P11 });
			}
		}
	}
	if (Triangles.Num() == 0)
	{
		Mesh->ClearMeshSection(0);
		return;
	}
	Mesh->CreateMeshSection(0, Vertices, Triangles, Normals, TArray<FVector2D>(), Colors, TArray<FProcMeshTangent>(), false);
	if (ChurnInks.IsValidIndex(Tile.Mesh) && ChurnInks[Tile.Mesh])
	{
		Mesh->SetMaterial(0, ChurnInks[Tile.Mesh].Get());
	}
}

int32 ACampaignMapActor::BuildChurnCraters(FFrontChurnTile& Tile, int32 Most)
{
	THEATER_SPEED_SCOPE(ChurnCraters);
	UProceduralMeshComponent* Mesh = ChurnMeshes.IsValidIndex(Tile.Mesh) ? ChurnMeshes[Tile.Mesh].Get() : nullptr;
	if (!Mesh)
	{
		return 0;
	}
	FFrontChurnTable& Table = *FrontChurn;
	const double U = UnitScale;
	// (the sun as the environment hangs it, toward which a large pit's floor lies in its own rim's shadow: see LayPenFront)
	const FVector Lamp = FRotator(-42.0, 69.0, 0.0).Vector();
	const FVector2D Sunward = FVector2D(-Lamp.Y, Lamp.X).GetSafeNormal(); // (map units: the opposite of where shadows fall)
	FMiniatureMesh Model;
	int32 Count = 0;
	for (const TheaterChurn::FPit& Pit : Tile.Pits)
	{
		if (Count >= Most)
		{
			break;
		}
		const double Show = TheaterChurn::CraterShown(2.0 * Pit.Radius * Tile.PixelsAUnit) * static_cast<double>(Pit.Dark);
		if (Show <= 0.03)
		{
			continue;
		}
		const int32 GI = FMath::Clamp(FMath::RoundToInt32((Pit.X - Terrain.OriginX) / Terrain.Cell), 0, Terrain.NX - 1);
		const int32 GJ = FMath::Clamp(FMath::RoundToInt32((Pit.Y - Terrain.OriginY) / Terrain.Cell), 0, Terrain.NY - 1);
		const int32 Index = Terrain.Index(GI, GJ);
		const int32 RegionId = Terrain.Region[Index];
		TheaterChurn::EGround Kind = TheaterChurn::EGround::Common;
		if (const int32* Near = Table.CellAt.Find(Index))
		{
			Kind = Table.Cells[*Near].Ground;
		}
		else if (Data.Regions.IsValidIndex(RegionId) && Data.Regions[RegionId].Terrain == TEXT("desert"))
		{
			Kind = TheaterChurn::EGround::Desert;
		}
		const uint8 SnowThere = Table.ShownMasks.IsValidIndex(Index) ? Table.ShownMasks[Index].G : 0;
		const TheaterChurn::ESeason Season = Table.MudSet.Contains(RegionId) ? TheaterChurn::ESeason::Mud : SnowThere > 77 ? TheaterChurn::ESeason::Snow : TheaterChurn::ESeason::Dry;
		uint8 Rim[3], Bottom[3], Shade[3];
		TheaterChurn::PitColours(Kind, Season, Pit, Rim, Bottom, Shade);
		const FLinearColor RimInk(FColor(Rim[0], Rim[1], Rim[2])), BottomInk(FColor(Bottom[0], Bottom[1], Bottom[2])), ShadeInk(FColor(Shade[0], Shade[1], Shade[2]));
		const double Grow = 0.6 + 0.4 * Show, R = Pit.Radius * U * Grow;
		const FVector Foot = SurfacePoint(Pit.X, Pit.Y, 0.0);
		if (Pit.bSpoil)
		{
			// A fleck of dry spoil: a low mound of it, all one colour.
			Model.Pillar(Foot - FVector(0.0, 0.0, 0.01 * U), R * 1.1, R * 0.8, 0.012 * U + R * 0.25 * Show, RimInk, 6);
		}
		else
		{
			// A crater: its rim (lit by the table's own light, so that its sunny side is the light one), the pit inside it,
			// and in a large one the third of the pit that lies in the shadow of its sunward rim.
			const double High = R * 0.3 * Show;
			Model.Pillar(Foot - FVector(0.0, 0.0, 0.3 * High), R * 1.35, R * 1.0, 1.3 * High, RimInk, 9);
			Model.Pillar(Foot + FVector(0.0, 0.0, 0.99 * High), R * 0.92, R * 0.9, 0.04 * High + 0.002 * U, BottomInk, 9);
			if (Pit.Radius >= 0.15)
			{
				const FVector2D Toward = Sunward * (Pit.Radius * Grow * 0.4); // (map units)
				Model.Pillar(Foot + FVector(0.0, 0.0, 1.02 * High) + MapToWorld(Toward.X, Toward.Y) - MapToWorld(0.0, 0.0), R * 0.5, R * 0.48, 0.03 * High + 0.002 * U, ShadeInk, 7);
			}
		}
		++Count;
	}
	if (Model.Vertices.Num() == 0)
	{
		Mesh->ClearMeshSection(1);
		return 0;
	}
	Mesh->CreateMeshSection(1, Model.Vertices, Model.Triangles, Model.Normals, TArray<FVector2D>(), Model.Colors, TArray<FProcMeshTangent>(), false);
	Mesh->SetMaterial(1, SceneryMaterial);
	return Count;
}

// ---- a frame --------------------------------------------------------------------------------------------------------

void ACampaignMapActor::UpdateFrontChurn(const FVector& CameraFocus, float CameraDistance)
{
	if (!FrontChurn.IsValid())
	{
		return; // (off, or no view since it came on: nothing at all)
	}
	FFrontChurnTable& Table = *FrontChurn;
	if (!FrontChurnWanted())
	{
		// Switched off in play: the ground given back at once, the meshes put away and the trees planted again.
		THEATER_SPEED_SCOPE(ChurnTakenUp);
		const bool bKept = Table.ShownGround.Num() == Paint.Ground.Num() && Table.ShownMasks.Num() == Paint.Masks.Num();
		if (bKept && Table.Cells.Num() > 0)
		{
			for (const FFrontChurnCell& Cell : Table.Cells)
			{
				Table.ShownGround[Cell.Index] = Cell.In;
				Table.ShownMasks[Cell.Index].G = Cell.SnowIn;
			}
			CampaignTextures::Update(GroundPicture, Table.ShownGround);
			CampaignTextures::Update(MasksPicture, Table.ShownMasks);
		}
		for (TPair<FIntPoint, FFrontChurnTile>& Each : Table.Tiles)
		{
			PutChurnTileAway(Each.Value);
		}
		for (int32 Mesh = 0; Mesh < TreeSquareOf.Num(); ++Mesh)
		{
			if (Table.TreeSquares.Contains(TreeSquareOf[Mesh]))
			{
				TreesDue.AddUnique(Mesh);
			}
		}
		FrontChurn.Reset();
		UE_LOG(LogTheater, Log, TEXT("Churned ground: switched off in play, the ground given back"));
		return;
	}
	if (!GroundPicture || !MasksPicture || Paint.Ground.Num() != Terrain.NX * Terrain.NY || Paint.Masks.Num() != Paint.Ground.Num()
		|| Terrain.Region.Num() != Paint.Ground.Num() || Terrain.Coverage.Num() != Paint.Ground.Num())
	{
		return; // (the table is not laid yet)
	}
	THEATER_SPEED_SCOPE(ChurnFrame);
	// The pictures as the table holds them now, kept from the first frame on (nothing of the churn is in them yet).
	if (Table.ShownGround.Num() != Paint.Ground.Num())
	{
		Table.ShownGround = SeasonGroundShown();
	}
	if (Table.ShownMasks.Num() != Paint.Masks.Num())
	{
		SeasonMasksShown(Table.ShownMasks);
	}
	// The rules' mud: where it lies the band takes the mud months' earth.
	uint32 MudNow = 2166136261u;
	for (const int32 RegionId : ShownMud)
	{
		MudNow = (MudNow ^ static_cast<uint32>(RegionId)) * 16777619u;
	}
	if (MudNow != Table.MudFor)
	{
		Table.MudFor = MudNow;
		Table.MudSet = TSet<int32>(ShownMud);
		for (FFrontChurnCell& Cell : Table.Cells)
		{
			LayChurnCell(Cell);
			Table.GroundDue.Add(Cell.Square);
		}
	}
	if (Table.bLayDue)
	{
		Table.bLayDue = false;
		LayFrontChurn();
	}

	// The eye: how many pixels of the window one unit of the map takes, at each place.
	FVector Eye = CameraFocus + FVector(0.0, 0.0, CameraDistance);
	double PixelsAtOne = 1500.0;
	if (UWorld* World = GetWorld())
	{
		if (const APlayerController* Player = World->GetFirstPlayerController())
		{
			if (Player->PlayerCameraManager)
			{
				Eye = Player->PlayerCameraManager->GetCameraLocation();
				const double Field = FMath::Clamp(static_cast<double>(Player->PlayerCameraManager->GetFOVAngle()), 5.0, 150.0);
				int32 WindowWide = 0, WindowHigh = 0;
				Player->GetViewportSize(WindowWide, WindowHigh);
				if (WindowWide > 0)
				{
					PixelsAtOne = WindowWide / (2.0 * FMath::Tan(FMath::DegreesToRadians(Field * 0.5)));
				}
			}
		}
	}
	auto PixelsAUnitAt = [&](double X, double Y, double& Far) -> double
	{
		Far = FMath::Max(FVector::Distance(Eye, MapToWorld(X, Y, 0.0)), 1.0);
		return PixelsAtOne * UnitScale / Far;
	};

	// ---- the picture's squares: each shows the band as far as it is wide on the screen there (in eighths) ----
	for (TPair<int32, uint8>& Square : Table.SquareShown)
	{
		const int32 SX = Square.Key % Table.SquaresAcross, SY = Square.Key / Table.SquaresAcross;
		const double X = Terrain.OriginX + (SX + 0.5) * ChurnPictureSquare * Terrain.Cell;
		const double Y = Terrain.OriginY + (SY + 0.5) * ChurnPictureSquare * Terrain.Cell;
		double Far = 0.0;
		const double Want = 8.0 * TheaterChurn::Shown(ChurnBandWide * PixelsAUnitAt(X, Y, Far));
		if (FMath::Abs(Want - static_cast<double>(Square.Value)) > 0.6)
		{
			Square.Value = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt32(Want), 0, 8));
			for (FFrontChurnCell& Cell : Table.Cells)
			{
				if (Cell.Square == Square.Key)
				{
					LayChurnCell(Cell);
				}
			}
			Table.GroundDue.Add(Square.Key);
			Table.MasksDue.Add(Square.Key);
		}
	}
	int32 Sends = 0;
	for (TSet<int32>* Due : { &Table.GroundDue, &Table.MasksDue })
	{
		while (Sends < ChurnSendsAFrame && Due->Num() > 0)
		{
			const int32 Square = *Due->CreateConstIterator();
			Due->Remove(Square);
			const int32 SX = Square % Table.SquaresAcross, SY = Square / Table.SquaresAcross;
			const int32 X0 = SX * ChurnPictureSquare, Y0 = SY * ChurnPictureSquare;
			const int32 X1 = FMath::Min(X0 + ChurnPictureSquare, Terrain.NX) - 1, Y1 = FMath::Min(Y0 + ChurnPictureSquare, Terrain.NY) - 1;
			if (X0 >= Terrain.NX || Y0 >= Terrain.NY)
			{
				continue;
			}
			if (Due == &Table.GroundDue)
			{
				CampaignTextures::UpdateBoxAll(GroundPicture, Table.ShownGround, X0, Y0, X1, Y1, ChurnPictureLevels);
			}
			else
			{
				CampaignTextures::UpdateBoxAll(MasksPicture, Table.ShownMasks, X0, Y0, X1, Y1, ChurnPictureLevels);
			}
			++Sends;
		}
	}

	// ---- the veil's squares near the eye: worked out on other threads, laid one a frame; craters nearest first ----
	if (Table.Tiles.Num() == 0)
	{
		return;
	}
	const double Now = FPlatformTime::Seconds();
	const FVector2D Focus = WorldToMap(CameraFocus);
	const double Reach = FMath::Max(48.0, 2.2 * static_cast<double>(CameraDistance) / UnitScale) + ChurnVeilSquare;
	TArray<FIntPoint> Near;
	for (TPair<FIntPoint, FFrontChurnTile>& Each : Table.Tiles)
	{
		FFrontChurnTile& Tile = Each.Value;
		const double X = (Each.Key.X + 0.5) * ChurnVeilSquare, Y = (Each.Key.Y + 0.5) * ChurnVeilSquare;
		Tile.PixelsAUnit = PixelsAUnitAt(X, Y, Tile.Far);
		const float Show = static_cast<float>(TheaterChurn::Shown(ChurnBandWide * Tile.PixelsAUnit));
		if (Show > 0.0f && FVector2D::Distance(FVector2D(X, Y), Focus) < Reach)
		{
			Near.Add(Each.Key);
			Tile.AwaySince = -1.0;
		}
		else if (Tile.Mesh != INDEX_NONE || Tile.Work.IsValid())
		{
			// (put away once it has been out of sight a couple of seconds: a glance back costs nothing)
			if (Tile.AwaySince < 0.0)
			{
				Tile.AwaySince = Now;
			}
			else if (Now - Tile.AwaySince > 2.0)
			{
				PutChurnTileAway(Tile);
			}
			if (Tile.Mesh != INDEX_NONE && Tile.Shown != 0.0f && ChurnInks.IsValidIndex(Tile.Mesh) && ChurnInks[Tile.Mesh])
			{
				Tile.Shown = 0.0f;
				ChurnInks[Tile.Mesh]->SetScalarParameterValue(TEXT("Opacity"), 0.0f);
			}
		}
	}
	Near.Sort([&Table](const FIntPoint& A, const FIntPoint& B) { return Table.Tiles.FindChecked(A).Far < Table.Tiles.FindChecked(B).Far; });
	bool bVeilLaid = false, bCratersLaid = false;
	int32 CratersLeft = FMath::Clamp(CVarFrontChurnCraters.GetValueOnGameThread(), 0, 3000);
	for (const FIntPoint& Key : Near)
	{
		FFrontChurnTile& Tile = Table.Tiles.FindChecked(Key);
		// Its mesh: a spare one, or a new one with its own ink.
		if (Tile.Mesh == INDEX_NONE)
		{
			if (Table.SpareMeshes.Num() > 0)
			{
				Tile.Mesh = Table.SpareMeshes.Pop();
			}
			else if (HighlightMaterial)
			{
				UProceduralMeshComponent* Made = UTheaterStillMesh::Make(this, TEXT("Churn"));
				Made->SetupAttachment(Root);
				Made->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Made->SetCastShadow(false);
				Made->RegisterComponent();
				UMaterialInstanceDynamic* Ink = UMaterialInstanceDynamic::Create(HighlightMaterial, this);
				Ink->SetScalarParameterValue(TEXT("Brightness"), 1.0f);
				Ink->SetScalarParameterValue(TEXT("Opacity"), 0.0f);
				Tile.Mesh = ChurnMeshes.Add(Made);
				ChurnInks.Add(Ink);
			}
			if (Tile.Mesh == INDEX_NONE)
			{
				continue;
			}
			ChurnMeshes[Tile.Mesh]->SetVisibility(true);
			Tile.Shown = -1.0f;
		}
		// Its veil: asked of another thread, and laid when it is back (one a frame).
		if (!Tile.bVeil)
		{
			if (Tile.Work.IsValid() && Tile.Work->bDone.load(std::memory_order_acquire))
			{
				const TSharedPtr<FFrontChurnVeilWork, ESPMode::ThreadSafe> Done = Tile.Work;
				if (Done->Gen != Table.Gen)
				{
					Tile.Work.Reset(); // (for lines since laid again: asked again below)
					Table.Working = FMath::Max(0, Table.Working - 1);
				}
				else if (!bVeilLaid)
				{
					bVeilLaid = true;
					Tile.Work.Reset();
					Table.Working = FMath::Max(0, Table.Working - 1);
					BuildChurnVeil(Tile, *Done);
					Tile.bVeil = true;
				}
			}
			if (!Tile.bVeil && !Tile.Work.IsValid() && Table.Working < ChurnWorkersAtOnce && Table.Lines.IsValid())
			{
				TSharedPtr<FFrontChurnVeilWork, ESPMode::ThreadSafe> Asked = MakeShared<FFrontChurnVeilWork, ESPMode::ThreadSafe>();
				Asked->Lines = Table.Lines;
				Asked->Gen = Table.Gen;
				Asked->OriginX = Key.X * ChurnVeilSquare;
				Asked->OriginY = Key.Y * ChurnVeilSquare;
				Asked->Across = FMath::RoundToInt32(ChurnVeilSquare / ChurnVeilStep) + 1;
				// (the land round it, from the terrain's own points, and the towns near it: copies, so that the thread
				// touches nothing of the map's)
				const int32 LI0 = FMath::Clamp(FMath::FloorToInt32((Asked->OriginX - Terrain.OriginX) / Terrain.Cell) - 1, 0, Terrain.NX - 1);
				const int32 LJ0 = FMath::Clamp(FMath::FloorToInt32((Asked->OriginY - Terrain.OriginY) / Terrain.Cell) - 1, 0, Terrain.NY - 1);
				const int32 LI1 = FMath::Clamp(FMath::CeilToInt32((Asked->OriginX + ChurnVeilSquare - Terrain.OriginX) / Terrain.Cell) + 1, 0, Terrain.NX - 1);
				const int32 LJ1 = FMath::Clamp(FMath::CeilToInt32((Asked->OriginY + ChurnVeilSquare - Terrain.OriginY) / Terrain.Cell) + 1, 0, Terrain.NY - 1);
				Asked->LandNX = LI1 - LI0 + 1;
				Asked->LandNY = LJ1 - LJ0 + 1;
				Asked->LandCell = Terrain.Cell;
				Asked->LandOriginX = Terrain.OriginX + LI0 * Terrain.Cell;
				Asked->LandOriginY = Terrain.OriginY + LJ0 * Terrain.Cell;
				Asked->Land.SetNumUninitialized(Asked->LandNX * Asked->LandNY);
				TSet<int32> Provinces;
				for (int32 LJ = LJ0; LJ <= LJ1; ++LJ)
				{
					for (int32 LI = LI0; LI <= LI1; ++LI)
					{
						Asked->Land[(LJ - LJ0) * Asked->LandNX + (LI - LI0)] = Terrain.Coverage[Terrain.Index(LI, LJ)];
						Provinces.Add(Terrain.Region[Terrain.Index(LI, LJ)]);
					}
				}
				for (const int32 RegionId : Provinces)
				{
					if (Data.Regions.IsValidIndex(RegionId))
					{
						const FVector2D& Town = Data.Regions[RegionId].Center;
						Asked->Towns.Add(FVector(Town.X, Town.Y, TownRadius(RegionId) + 0.6));
					}
				}
				Tile.Work = Asked;
				++Table.Working;
				Tile.Building = Async(EAsyncExecution::ThreadPool, [Asked]()
				{
					Asked->Work();
					Asked->bDone.store(true, std::memory_order_release);
				});
			}
		}
		// Faded in as far as the band is wide on the screen.
		const float Show = static_cast<float>(TheaterChurn::Shown(ChurnBandWide * Tile.PixelsAUnit));
		if (Tile.bVeil && FMath::Abs(Show - Tile.Shown) > 0.02f && ChurnInks.IsValidIndex(Tile.Mesh) && ChurnInks[Tile.Mesh])
		{
			Tile.Shown = Show;
			ChurnInks[Tile.Mesh]->SetScalarParameterValue(TEXT("Opacity"), Show);
		}
		// Its craters, for the size its pixels are now (in half octaves), while the most in view allows.
		const int32 Size = FMath::RoundToInt32(2.0 * std::log2(FMath::Max(Tile.PixelsAUnit, 1.0e-3)));
		int32 Wanted = 0;
		for (const TheaterChurn::FPit& Pit : Tile.Pits)
		{
			Wanted += TheaterChurn::CraterShown(2.0 * Pit.Radius * Tile.PixelsAUnit) * Pit.Dark > 0.03 ? 1 : 0;
		}
		Wanted = FMath::Min(Wanted, CratersLeft);
		if (Tile.CratersFor != Size || Tile.Craters != Wanted)
		{
			if (Wanted == 0)
			{
				if (Tile.Craters > 0 || Tile.CratersFor != INDEX_NONE)
				{
					ChurnMeshes[Tile.Mesh]->ClearMeshSection(1);
				}
				Tile.Craters = 0;
				Tile.CratersFor = Size;
			}
			else if (Tile.bVeil && !bCratersLaid)
			{
				bCratersLaid = true;
				Tile.Craters = BuildChurnCraters(Tile, Wanted);
				Tile.CratersFor = Size;
			}
		}
		CratersLeft = FMath::Max(0, CratersLeft - Tile.Craters);
	}
}
