// The general's controls on a live ground battle (release 2; Brian, 22 November 2026: "im the general who controls their
// deployment and who to attack and move on a map with tactics"; "i also want the game play to be smooth, fun, and
// realistic"). The mock-up he saw (general-controls-mockup.html): click a unit to pick it up, Shift-click to add more;
// click the ground and they go there, click an enemy and they attack it; drag on the ground for a box round several, a
// right-drag for a line; before the battle, drag his units about his own ground; Space pauses (orders still go), 1 2 3
// the speeds, H hold, R fall back, Tab his next unit, C the film, Esc the map again.
//
// ALandBattle's part that turns the screen's pointer into the clipboard's words (LandCommand) and the clipboard's orders
// into the rules' (OrderLive), and puts each block's tag, its route and his drag on the screen for SLandBattleScreen to
// draw. Every order is shown the moment it is given (the screen runs a couple of battle seconds behind the rules, so
// the battle itself only shows it a moment later): a mark where he clicked, the route, the tag's RADIO chip.

#include "Battle/LandBattle.h"
#include "Battle/LandBattleScreen.h"
#include "Battle/LandLive.h"

#include "Battle/LandBattleState.h"
#include "Battle/LandFell.h"
#include "Battle/LandScript.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "TheaterCommand.h"
#include "UnrealClient.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

namespace
{
	std::string NarrowOf(const FString& Text)
	{
		const FTCHARToUTF8 Utf8(*Text);
		return std::string(Utf8.Get(), static_cast<size_t>(Utf8.Length()));
	}

	FString WideOf(const std::string& Text)
	{
		return FString(UTF8_TO_TCHAR(Text.c_str()));
	}

	/** How high over the ground a block's tag hangs on its stalk, metres. */
	constexpr double TagLift = 14.0;
	/** A press that moves this far, pixels, is a drag. */
	constexpr double DragPixels = 6.0;
	/** A second click this soon (real seconds) and this near (pixels) where an order went sends them at the double. */
	constexpr double TwiceSeconds = 0.4, TwicePixels = 12.0;
	/** Half a smoke screen laid at a click, metres (a platoon's 2-inch mortars and grenades screen a front of about 150 m;
	 * a line dragged sets its own). */
	constexpr double SmokeHalfWidth = 80.0;

	/** Whether the rules' cell under a place is woods (a block standing there at the start is hidden). */
	bool WoodsAt(const LandFeed::Field& F, double X, double Y)
	{
		if (F.Cols <= 0 || F.Rows <= 0 || F.Cell <= 0.0 || F.Ground.size() < static_cast<size_t>(F.Cols) * static_cast<size_t>(F.Rows))
		{
			return false;
		}
		const int Col = FMath::Clamp(static_cast<int>(std::floor(X / F.Cell)), 0, F.Cols - 1);
		const int Row = FMath::Clamp(static_cast<int>(std::floor(Y / F.Cell)), 0, F.Rows - 1);
		const size_t Kind = F.Ground[static_cast<size_t>(Row) * static_cast<size_t>(F.Cols) + static_cast<size_t>(Col)];
		return Kind < F.Grounds.size() && F.Grounds[Kind] == "woods";
	}

	/** The woods' edges within a box (x0, y0, x1, y1, field metres): where a woods cell meets one that is not, along a
	 * row or column of cells, joined into runs. Two points a stretch. */
	std::vector<std::pair<double, double>> WoodsEdges(const LandFeed::Field& F, const double* Box)
	{
		std::vector<std::pair<double, double>> Out;
		if (F.Cols <= 0 || F.Rows <= 0 || F.Cell <= 0.0 || F.Ground.size() < static_cast<size_t>(F.Cols) * static_cast<size_t>(F.Rows))
		{
			return Out;
		}
		auto Woods = [&F](int Col, int Row)
		{
			if (Col < 0 || Row < 0 || Col >= F.Cols || Row >= F.Rows)
			{
				return false;
			}
			const size_t Kind = F.Ground[static_cast<size_t>(Row) * static_cast<size_t>(F.Cols) + static_cast<size_t>(Col)];
			return Kind < F.Grounds.size() && F.Grounds[Kind] == "woods";
		};
		const int C0 = FMath::Max(0, static_cast<int>(std::floor(Box[0] / F.Cell))), C1 = FMath::Min(F.Cols - 1, static_cast<int>(std::floor(Box[2] / F.Cell)));
		const int R0 = FMath::Max(0, static_cast<int>(std::floor(Box[1] / F.Cell))), R1 = FMath::Min(F.Rows - 1, static_cast<int>(std::floor(Box[3] / F.Cell)));
		// (the lines between rows: a run along x where the cell below and the cell above differ)
		for (int Row = R0; Row <= R1 + 1; ++Row)
		{
			int Start = -1;
			for (int Col = C0; Col <= C1 + 1; ++Col)
			{
				const bool bEdge = Col <= C1 && Woods(Col, Row - 1) != Woods(Col, Row);
				if (bEdge && Start < 0) Start = Col;
				if (!bEdge && Start >= 0)
				{
					Out.push_back({ Start * F.Cell, Row * F.Cell });
					Out.push_back({ Col * F.Cell, Row * F.Cell });
					Start = -1;
				}
			}
		}
		// (the lines between columns: a run along y)
		for (int Col = C0; Col <= C1 + 1; ++Col)
		{
			int Start = -1;
			for (int Row = R0; Row <= R1 + 1; ++Row)
			{
				const bool bEdge = Row <= R1 && Woods(Col - 1, Row) != Woods(Col, Row);
				if (bEdge && Start < 0) Start = Row;
				if (!bEdge && Start >= 0)
				{
					Out.push_back({ Col * F.Cell, Start * F.Cell });
					Out.push_back({ Col * F.Cell, Row * F.Cell });
					Start = -1;
				}
			}
		}
		return Out;
	}

	FVector2D ViewSize()
	{
		FVector2D Size(1600.0, 900.0);
		if (GEngine && GEngine->GameViewport)
		{
			GEngine->GameViewport->GetViewportSize(Size);
		}
		return FVector2D(FMath::Max(1.0, Size.X), FMath::Max(1.0, Size.Y));
	}
}

// ---- The screen and the ground ---------------------------------------------------------------------------------

bool ALandBattle::PixelOf(double X, double Y, double Lift, FVector2D& OutPixel) const
{
	// Through the battle's own eye as it stands this frame (not the view the renderer drew last frame), so a tag stays on
	// its block while the camera moves.
	if (!Eye || !State.IsValid())
	{
		return false;
	}
	const UCameraComponent* Lens = Eye->GetCameraComponent();
	const FVector Local = Lens->GetComponentTransform().InverseTransformPositionNoScale(World(X, Y, GroundZ(X, Y) + Lift));
	if (Local.X <= 10.0)
	{
		return false;
	}
	const FVector2D View = ViewSize();
	const double HalfTan = FMath::Tan(FMath::DegreesToRadians(static_cast<double>(Lens->FieldOfView) * 0.5));
	const double Across = Local.Y / (Local.X * HalfTan);
	const double Up = Local.Z / (Local.X * HalfTan) * (View.X / View.Y);
	OutPixel = FVector2D((Across + 1.0) * 0.5 * View.X, (1.0 - Up) * 0.5 * View.Y);
	return true;
}

bool ALandBattle::GroundUnder(const FVector2D& Pixel, double& OutX, double& OutY) const
{
	if (!Eye || !State.IsValid() || !State->Field)
	{
		return false;
	}
	const UCameraComponent* Lens = Eye->GetCameraComponent();
	const FVector2D View = ViewSize();
	const double HalfTan = FMath::Tan(FMath::DegreesToRadians(static_cast<double>(Lens->FieldOfView) * 0.5));
	const double Across = (Pixel.X / View.X * 2.0 - 1.0) * HalfTan;
	const double Up = (1.0 - Pixel.Y / View.Y * 2.0) * HalfTan * (View.Y / View.X);
	const FTransform& Frame = Lens->GetComponentTransform();
	const FVector WayWorld = Frame.TransformVectorNoScale(FVector(1.0, Across, Up)).GetSafeNormal();
	const FVector FromWorld = Frame.GetLocation();
	// (the field's own axes: x east is the world's Y, y north the world's X; metres)
	const FVector From(FromWorld.Y / UnitsPerMetre, FromWorld.X / UnitsPerMetre, FromWorld.Z / UnitsPerMetre);
	const FVector Way(WayWorld.Y, WayWorld.X, WayWorld.Z);
	const LandField::Ground& Field = *State->Field;
	auto Over = [&](double Along)
	{
		const FVector At = From + Way * Along;
		return At.Z - Field.HeightAt(At.X, At.Y);
	};
	if (Over(0.0) <= 0.0)
	{
		return false;
	}
	// Along the ray in steps that grow with the distance, then halved down onto the ground.
	double Before = 0.0, Along = 1.0;
	bool bHit = false;
	while (Along < 9000.0)
	{
		if (Over(Along) <= 0.0)
		{
			bHit = true;
			break;
		}
		Before = Along;
		Along += FMath::Clamp(Along * 0.015, 0.75, 20.0);
	}
	if (!bHit)
	{
		return false;
	}
	for (int32 K = 0; K < 14; ++K)
	{
		const double Mid = (Before + Along) * 0.5;
		(Over(Mid) > 0.0 ? Before : Along) = Mid;
	}
	const FVector At = From + Way * Along;
	const LandFeed::Field& Ground = State->Rec.Ground;
	if (At.X < 0.0 || At.Y < 0.0 || At.X > Ground.W || At.Y > Ground.H)
	{
		return false;
	}
	OutX = At.X;
	OutY = At.Y;
	return true;
}

double ALandBattle::ReachAt(const FVector2D& Pixel) const
{
	double X = 0.0, Y = 0.0;
	if (!Eye || !GroundUnder(Pixel, X, Y))
	{
		return 20.0;
	}
	const double Far = (World(X, Y, GroundZ(X, Y)) - Eye->GetCameraComponent()->GetComponentLocation()).Size() / UnitsPerMetre;
	const double HalfTan = FMath::Tan(FMath::DegreesToRadians(static_cast<double>(Eye->GetCameraComponent()->FieldOfView) * 0.5));
	const double PerPixel = 2.0 * Far * HalfTan / ViewSize().X;
	return FMath::Clamp(PerPixel * 24.0, 3.0, 150.0);
}

// ---- Each frame --------------------------------------------------------------------------------------------------

void ALandBattle::ShowGeneral()
{
	if (!State.IsValid())
	{
		return;
	}
	FLandBattleState& S = *State;
	FLandGeneralShown& G = S.General;
	G.bOn = S.Rec.bLive && S.Rec.Me >= 0;
	G.Marks.clear();
	G.Routes.clear();
	G.RoomEdge.clear();
	G.WoodsEdge.clear();
	G.Comers.clear();
	G.Sights.clear();
	G.SightWords.clear();
	G.bSights = false;
	if (!G.bOn)
	{
		G.FoeHidden.clear();
		return;
	}
	LandFell::Phase("general: the clipboard");
	G.Clipboard.Update(S.Rec, S.Battle);
	// (the enemy's hidden from him: kept off the screen in the film too, so it never gives the ambush away)
	G.FoeHidden.clear();
	for (const LandCommand::Tag& T : G.Clipboard.Tags())
	{
		if (!T.bMine && T.bHidden && T.Says != LandCommand::Chip::Gone)
		{
			G.FoeHidden.push_back(T.Block);
		}
	}
	std::sort(G.FoeHidden.begin(), G.FoeHidden.end());
	const FVector2D View = ViewSize();
	G.ViewW = View.X;
	G.ViewH = View.Y;
	if (G.bFilm)
	{
		return;
	}
	LandFell::Phase("general: the tags and routes");
	const std::vector<LandCommand::Tag>& Tags = G.Clipboard.Tags();
	// (each unit's ground washed from high up, fading as the eye comes down: full at 300 m over the ground, gone by 150 m)
	const double EyeOver = S.Pose.Eye.Z - S.Rec.Ground.HeightAt(S.Pose.Eye.X, S.Pose.Eye.Y);
	G.PrintInk = FMath::Clamp((EyeOver - 150.0) / 150.0, 0.0, 1.0) * 0.25;
	auto OnView = [&View](const FVector2D& P, double Margin) { return P.X > -Margin && P.Y > -Margin && P.X < View.X + Margin && P.Y < View.Y + Margin; };
	// A line along the ground, a point every few metres so it lies over the rises: broken where it goes behind the eye.
	auto Along = [this](std::vector<std::pair<double, double>>& Into, double X0, double Y0, double X1, double Y1, double Lift)
	{
		const double Long = FMath::Sqrt((X1 - X0) * (X1 - X0) + (Y1 - Y0) * (Y1 - Y0));
		const int32 Steps = FMath::Clamp(static_cast<int32>(Long / 20.0), 1, 60);
		for (int32 K = Into.empty() ? 0 : 1; K <= Steps; ++K)
		{
			const double F = static_cast<double>(K) / Steps;
			FVector2D P;
			if (PixelOf(X0 + (X1 - X0) * F, Y0 + (Y1 - Y0) * F, Lift, P))
			{
				Into.push_back({ P.X, P.Y });
			}
		}
	};
	// The spotter's mark (release 2, -TheaterLandSpotter): looking down on the field, a ring round each of his fighting
	// blocks, inside which a hidden enemy is seen (the rules' distances, LandSight::Finds). Every block's ring for
	// hidden men on foot; a block in hand's for guns and teams and for tanks and motorised blocks too, dashed, each
	// with its word. Not while he sets his army out: nothing is found before the clock starts, and the setup's own
	// words stand where the mark's would.
	if (SpotterOn < 0)
	{
		SpotterOn = FParse::Param(FCommandLine::Get(), TEXT("TheaterLandSpotter")) && FLandLive::RuleNow(TEXT("hide")) ? 1 : 0;
		UE_LOG(LogTheater, Display, TEXT("Land battle (spotter's mark): %s"), SpotterOn == 1 ? TEXT("on: rings in the top view") : TEXT("off (no -TheaterLandSpotter, or Forest ambushes off)"));
	}
	if (SpotterOn == 1 && S.bGeneralEye && !G.Clipboard.Deploying() && (S.GeneralEye.TopView() || (S.GeneralEye.OwnTilt() && S.GeneralEye.TiltDegrees() >= 60.0)))
	{
		G.bSights = true;
		const LandSight::Finds Finds;
		const double Sight[3] = { Finds.Foot, Finds.Guns, Finds.Vehicles };
		const int32 Around = 48;
		for (const LandCommand::Tag& T : Tags)
		{
			if (!T.bMine || T.Says == LandCommand::Chip::Gone || T.State == "routing" || T.State == "gone" || T.State == "withdrawn")
			{
				continue;
			}
			for (int32 Kind = 0; Kind < (T.bChosen ? 3 : 1); ++Kind)
			{
				const double Reach = LandSight::FindsWithin(Sight[Kind], T.W, T.D);
				// (a dashed ring for guns and vehicles: so many stretches drawn, so many left out)
				const int32 DashOn = Kind == 0 ? Around : Kind == 1 ? 2 : 1, DashOff = Kind == 0 ? 0 : 1;
				FLandGeneralShown::SightStroke Stroke;
				Stroke.Block = T.Block;
				Stroke.Kind = Kind;
				Stroke.bChosen = T.bChosen;
				bool bSeen = false, bWord = false, bWas = false;
				FVector2D Was(0.0, 0.0), WordAt(-1.0e9, 0.0);
				auto Flush = [&]()
				{
					// (a run with no stretch of it on or across the screen is not drawn)
					if (Stroke.Points.size() >= 2 && bSeen)
					{
						G.Sights.push_back(Stroke);
					}
					Stroke.Points.clear();
					bSeen = false;
				};
				for (int32 K = 0; K <= Around; ++K)
				{
					const double Turn = 2.0 * UE_DOUBLE_PI * K / Around;
					FVector2D P(0.0, 0.0);
					const bool bOk = PixelOf(T.X + Reach * FMath::Sin(Turn), T.Y + Reach * FMath::Cos(Turn), 0.4, P);
					// (the stretch from the point before to this one: drawn where both are before the eye and it is not a
					// gap in the dashes)
					if (K > 0 && bOk && bWas && (K - 1) % (DashOn + DashOff) < DashOn)
					{
						if (Stroke.Points.empty())
						{
							Stroke.Points.push_back({ Was.X, Was.Y });
						}
						Stroke.Points.push_back({ P.X, P.Y });
						// (judged by the stretch, not by its ends: zoomed in, a ring crosses the screen with both ends of
						// a stretch off it, and either end is then within the stretch's own length of the screen)
						bSeen = bSeen || OnView(P, FVector2D::Distance(Was, P));
					}
					else
					{
						Flush();
					}
					// (its word by the rightmost of the ring that is well on the screen)
					if (bOk && T.bChosen && OnView(P, -70.0) && P.X > WordAt.X)
					{
						WordAt = P;
						bWord = true;
					}
					Was = P;
					bWas = bOk;
				}
				Flush();
				if (bWord)
				{
					G.SightWords.push_back({ Kind, Sight[Kind], WordAt.X, WordAt.Y });
				}
			}
		}
	}
	for (const LandCommand::Tag& T : Tags)
	{
		// (gone; or the enemy's, hidden in woods and not yet found: nothing of it shows)
		if (T.Says == LandCommand::Chip::Gone || (!T.bMine && T.bHidden))
		{
			continue;
		}
		FLandGeneralShown::Mark M;
		M.Block = T.Block;
		FVector2D Top, Foot;
		if (PixelOf(T.X, T.Y, TagLift, Top) && PixelOf(T.X, T.Y, 0.4, Foot) && OnView(Top, 120.0))
		{
			M.X = Top.X;
			M.Y = Top.Y;
			M.FootX = Foot.X;
			M.FootY = Foot.Y;
			if (T.bChosen)
			{
				// (the ring round a block in hand: its ground's corners, across its facing and along it)
				const double S1 = FMath::Sin(T.Face), C1 = FMath::Cos(T.Face);
				const double Hw = FMath::Max(T.W, 20.0) * 0.5, Hd = FMath::Max(T.D, 12.0) * 0.5;
				const double Corner[4][2] = { { -Hw, -Hd }, { Hw, -Hd }, { Hw, Hd }, { -Hw, Hd } };
				M.bCorners = true;
				for (int32 K = 0; K < 4; ++K)
				{
					const double Cx = T.X + Corner[K][0] * C1 + Corner[K][1] * S1, Cy = T.Y - Corner[K][0] * S1 + Corner[K][1] * C1;
					FVector2D P;
					M.bCorners = M.bCorners && PixelOf(Cx, Cy, 0.5, P);
					M.Corners[K * 2] = P.X;
					M.Corners[K * 2 + 1] = P.Y;
				}
			}
			if (G.PrintInk > 0.01 && T.bKnown && T.W > 1.0 && T.D > 1.0)
			{
				// (its real frontage and depth on the ground, across its facing and along it)
				const double Sn = FMath::Sin(T.Face), Cs = FMath::Cos(T.Face);
				const double Pw = T.W * 0.5, Pd = T.D * 0.5;
				const double Edge[4][2] = { { -Pw, -Pd }, { Pw, -Pd }, { Pw, Pd }, { -Pw, Pd } };
				M.bPrint = true;
				for (int32 K = 0; K < 4; ++K)
				{
					const double Cx = T.X + Edge[K][0] * Cs + Edge[K][1] * Sn, Cy = T.Y - Edge[K][0] * Sn + Edge[K][1] * Cs;
					FVector2D P;
					M.bPrint = M.bPrint && PixelOf(Cx, Cy, 0.3, P);
					M.Print[K * 2] = P.X;
					M.Print[K * 2 + 1] = P.Y;
				}
			}
			G.Marks.push_back(M);
		}
		// His own firing into cover: a line to where they fire (reconnaissance by fire).
		if (T.bMine && T.bSearch)
		{
			FLandGeneralShown::Route R;
			R.Block = T.Block;
			R.Says = T.Says == LandCommand::Chip::Radio ? LandCommand::Chip::Yours : T.Says;
			R.bAttack = true;
			R.bChosen = T.bChosen;
			Along(R.Points, T.X, T.Y, T.SearchX, T.SearchY, 0.6);
			if (R.Points.size() >= 2)
			{
				G.Routes.push_back(std::move(R));
			}
			continue;
		}
		// His orders' routes: those in hand, and those he has taken in hand (his staff's own are not drawn: too many).
		if (!T.bMine || T.OrderKind.empty() || T.OrderKind == "hold" || T.OrderKind == "smoke" || (!T.bChosen && T.Says == LandCommand::Chip::Staff))
		{
			continue;
		}
		FLandGeneralShown::Route R;
		R.Block = T.Block;
		R.Says = T.Says;
		R.bAttack = T.OrderKind == "attack";
		R.bBack = T.OrderKind == "withdraw";
		R.bChosen = T.bChosen;
		double Ex = T.OrderX, Ey = T.OrderY;
		if (R.bAttack && T.OrderTarget >= 0)
		{
			if (const LandCommand::Tag* Foe = G.Clipboard.TagOf(T.OrderTarget))
			{
				Ex = Foe->X;
				Ey = Foe->Y;
			}
		}
		double Px = T.X, Py = T.Y;
		for (const std::pair<double, double>& Corner : T.Path)
		{
			Along(R.Points, Px, Py, Corner.first, Corner.second, 0.6);
			Px = Corner.first;
			Py = Corner.second;
		}
		Along(R.Points, Px, Py, Ex, Ey, 0.6);
		if (R.Points.size() >= 2)
		{
			G.Routes.push_back(std::move(R));
		}
	}
	// His ground, while he stands his blocks on it; and the woods on and near it, where a block starts hidden.
	if (G.Clipboard.Deploying())
	{
		LandFell::Phase("general: his ground and the woods");
		const double* Room = G.Clipboard.Room();
		if (!G.bWoodsMade)
		{
			G.bWoodsMade = true;
			const double Near[4] = { Room[0] - 150.0, Room[1] - 150.0, Room[2] + 150.0, Room[3] + 150.0 };
			G.WoodsField = WoodsEdges(S.Rec.Ground, Near);
		}
		for (size_t K = 0; K + 1 < G.WoodsField.size(); K += 2)
		{
			// (a stretch cut in pieces of a cell or two, so it lies over the rises; those behind the eye left out)
			const std::pair<double, double>& A = G.WoodsField[K];
			const std::pair<double, double>& B = G.WoodsField[K + 1];
			const double Long = FMath::Sqrt((B.first - A.first) * (B.first - A.first) + (B.second - A.second) * (B.second - A.second));
			const int32 Steps = FMath::Clamp(static_cast<int32>(Long / 40.0), 1, 40);
			FVector2D Was;
			bool bWas = PixelOf(A.first, A.second, 0.6, Was);
			for (int32 Step = 1; Step <= Steps; ++Step)
			{
				const double F = static_cast<double>(Step) / Steps;
				FVector2D Now;
				const bool bNow = PixelOf(A.first + (B.first - A.first) * F, A.second + (B.second - A.second) * F, 0.6, Now);
				if (bWas && bNow && OnView(Was, 400.0) && OnView(Now, 400.0))
				{
					G.WoodsEdge.push_back({ Was.X, Was.Y });
					G.WoodsEdge.push_back({ Now.X, Now.Y });
				}
				Was = Now;
				bWas = bNow;
			}
		}
		Along(G.RoomEdge, Room[0], Room[1], Room[2], Room[1], 0.5);
		Along(G.RoomEdge, Room[2], Room[1], Room[2], Room[3], 0.5);
		Along(G.RoomEdge, Room[2], Room[3], Room[0], Room[3], 0.5);
		Along(G.RoomEdge, Room[0], Room[3], Room[0], Room[1], 0.5);
	}
	// The corps coming up from next door (neighbours join the fight, release 2): a chip over the place on the field's edge
	// where it will walk on when that is in sight, else just inside the screen's edge on the way to it, like the arrow at
	// a map's margin pointing to the next sheet.
	if (!G.Clipboard.Neighbours().empty() && Eye)
	{
		LandFell::Phase("general: the neighbours coming");
		const UCameraComponent* Lens = Eye->GetCameraComponent();
		const FTransform& Frame = Lens->GetComponentTransform();
		const double HalfTan = FMath::Tan(FMath::DegreesToRadians(static_cast<double>(Lens->FieldOfView) * 0.5));
		// (clear of the bars at the top, the desk and the order bar at the bottom, and wide enough for the chip at the sides)
		const double Left = 140.0, Right = FMath::Max(Left + 1.0, View.X - 140.0), Top = 112.0, Bottom = FMath::Max(Top + 1.0, View.Y * 0.70);
		const FVector2D Mid((Left + Right) * 0.5, (Top + Bottom) * 0.5);
		for (const LandCommand::Neighbour& N : G.Clipboard.Neighbours())
		{
			FLandGeneralShown::Comer C;
			C.What = N;
			const FVector Local = Frame.InverseTransformPositionNoScale(World(N.X, N.Y, GroundZ(N.X, N.Y) + 2.0));
			FVector2D P(Mid);
			const bool bAhead = Local.X > 10.0;
			if (bAhead)
			{
				P = FVector2D((Local.Y / (Local.X * HalfTan) + 1.0) * 0.5 * View.X, (1.0 - Local.Z / (Local.X * HalfTan) * (View.X / View.Y)) * 0.5 * View.Y);
			}
			C.bThere = bAhead && P.X >= Left && P.X <= Right && P.Y >= Top && P.Y <= Bottom;
			if (!C.bThere)
			{
				// (from the screen's middle toward it, stopped at the margin: behind the eye, its way across and down)
				FVector2D Way = bAhead ? P - Mid : FVector2D(Local.Y, -Local.Z);
				if (Way.SizeSquared() < 1e-6)
				{
					Way = FVector2D(0.0, 1.0);
				}
				Way.Normalize();
				const double ToX = Way.X > 1e-6 ? (Right - Mid.X) / Way.X : Way.X < -1e-6 ? (Left - Mid.X) / Way.X : 1e12;
				const double ToY = Way.Y > 1e-6 ? (Bottom - Mid.Y) / Way.Y : Way.Y < -1e-6 ? (Top - Mid.Y) / Way.Y : 1e12;
				P = Mid + Way * FMath::Min(ToX, ToY);
				C.WayX = Way.X;
				C.WayY = Way.Y;
			}
			// (two corps on the same stretch of margin: the later one a row lower)
			for (const FLandGeneralShown::Comer& Was : G.Comers)
			{
				if (FMath::Abs(Was.X - P.X) < 220.0 && FMath::Abs(Was.Y - P.Y) < 44.0)
				{
					P.Y = Was.Y + 46.0;
				}
			}
			C.X = P.X;
			C.Y = P.Y;
			G.Comers.push_back(std::move(C));
		}
	}
	// His despatch riders (the rider ahead, release 2): where each is on the screen; a despatch read out for 15 s, or till
	// his rider has gone (his corps come up).
	G.RiderMarks.clear();
	bool bReadStill = false;
	for (const LandFeed::Rider& R : G.Clipboard.Riders())
	{
		FVector2D P;
		bReadStill = bReadStill || R.Corps == G.RiderRead;
		if (PixelOf(R.X, R.Y, 2.5, P) && OnView(P, 40.0))
		{
			G.RiderMarks.push_back({ R.Corps, P.X, P.Y, R.Says });
		}
	}
	if (!bReadStill || RealT - G.RiderReadAt > 15.0)
	{
		G.RiderRead = -1;
	}
	// The mark answering his last order, a second and a bit.
	LandFell::Phase("general: the ping");
	FVector2D Ping;
	G.bPingOn = RealT - G.PingAt < 1.2 && PixelOf(G.PingFieldX, G.PingFieldY, 0.5, Ping);
	if (G.bPingOn)
	{
		G.PingX = Ping.X;
		G.PingY = Ping.Y;
		G.PingAge = RealT - G.PingAt;
	}
	// What a click would do under a still pointer (the battle moves under it).
	if (GeneralButton < 0 && G.PointerX >= 0.0)
	{
		LandFell::Phase("general: under the still pointer");
		GeneralMove(FVector2D(G.PointerX, G.PointerY), G.Hover);
	}
}

// ---- The pointer ----------------------------------------------------------------------------------------------

void ALandBattle::GeneralPress(const FVector2D& Pixel, int32 Button, bool bShift, int32 OnTag)
{
	(void)bShift;
	if (!State.IsValid() || !State->General.bOn || State->General.bFilm)
	{
		return;
	}
	FLandGeneralShown& G = State->General;
	GeneralDownAt = Pixel;
	GeneralButton = Button;
	bGeneralMoved = false;
	GeneralCarry = -1;
	// In the setup step a press on one of his blocks (or its tag) picks it up to carry.
	if (Button == 0 && G.Clipboard.Deploying())
	{
		int32 Block = -1;
		const LandCommand::Tag* On = OnTag >= 0 ? G.Clipboard.TagOf(OnTag) : nullptr;
		double X = 0.0, Y = 0.0;
		if (On && On->bOrderable)
		{
			Block = OnTag;
		}
		else if (GroundUnder(Pixel, X, Y))
		{
			Block = G.Clipboard.MineAt(X, Y, ReachAt(Pixel) * 0.5);
		}
		if (const LandCommand::Tag* Carried = Block >= 0 ? G.Clipboard.TagOf(Block) : nullptr)
		{
			GeneralCarry = Block;
			GeneralCarryFace = Carried->Face;
		}
	}
}

void ALandBattle::GeneralMove(const FVector2D& Pixel, int32 OnTag)
{
	if (!State.IsValid() || !State->General.bOn || State->General.bFilm)
	{
		return;
	}
	FLandGeneralShown& G = State->General;
	LandCommand::General& Clip = G.Clipboard;
	G.PointerX = Pixel.X;
	G.PointerY = Pixel.Y;
	G.Hover = OnTag;
	double X = 0.0, Y = 0.0;
	const bool bGround = GroundUnder(Pixel, X, Y);
	if (GeneralButton < 0)
	{
		// What a click would do here, said by the pointer.
		const double Reach = bGround ? ReachAt(Pixel) : 0.0;
		const LandCommand::Tag* On = OnTag >= 0 ? Clip.TagOf(OnTag) : nullptr;
		const int32 Mine = On ? (On->bOrderable ? OnTag : -1) : bGround ? Clip.MineAt(X, Y, Reach * 0.5) : -1;
		const int32 Foe = On ? (!On->bMine ? OnTag : -1) : bGround ? Clip.EnemyAt(X, Y, Reach) : -1;
		const std::vector<int> Hand = Clip.Chosen();
		G.bGapAt = false;
		// (an armed tool no one in hand still has, put down)
		if ((G.Armed == "gap" && Clip.Cutter() < 0) || (G.Armed == "smoke" && Clip.Smokers().empty()))
		{
			G.Armed.clear();
		}
		if (Clip.Deploying())
		{
			G.Would = Mine >= 0 ? "DRAG TO MOVE IT" : "";
		}
		else if (Mine >= 0)
		{
			G.Would = "PICK UP";
		}
		else if (G.Armed.empty() && bGround && Clip.RiderAt(X, Y, FMath::Max(Reach * 0.6, 15.0)) >= 0)
		{
			G.Would = "READ HIS DESPATCH";
		}
		else if (Hand.empty())
		{
			G.Would = "";
		}
		else if (G.Armed == "gap")
		{
			// (the cutters: the gap snaps to the hedge nearest the pointer, shown where it would be cut)
			double Gx = 0.0, Gy = 0.0;
			FVector2D At;
			G.bGapAt = bGround && HedgeNear(X, Y, FMath::Max(Reach, 30.0), Gx, Gy) && PixelOf(Gx, Gy, 0.8, At);
			G.GapAtX = At.X;
			G.GapAtY = At.Y;
			G.Would = !bGround ? "" : G.bGapAt ? "CUT A GAP HERE" : "NO HEDGE HERE";
		}
		else if (G.Armed == "smoke")
		{
			G.Would = bGround ? "LAY SMOKE HERE, OR DRAG ITS LINE" : "";
		}
		else if (Foe >= 0)
		{
			const bool bGuns = std::all_of(Hand.begin(), Hand.end(), [&Clip](int Id) { const LandCommand::Tag* T = Clip.TagOf(Id); return T && T->Kind == "art"; });
			G.Would = bGuns || G.Armed == "fire" ? "FIRE ON IT" : "ATTACK";
		}
		else
		{
			// (FIRE ON armed over the ground: reconnaissance by fire, into the wood or hedge there)
			G.Would = !bGround ? "" : G.Armed == "fire" ? (WoodsAt(State->Rec.Ground, X, Y) ? "FIRE INTO THE WOODS" : "FIRE INTO THE COVER")
				: G.Armed == "double" ? "GO AT THE DOUBLE" : "GO HERE";
		}
		return;
	}
	G.Would.clear();
	if ((Pixel - GeneralDownAt).Size() > DragPixels)
	{
		bGeneralMoved = true;
	}
	if (!bGeneralMoved)
	{
		return;
	}
	if (GeneralCarry >= 0)
	{
		// The block carried: its ground where it would stand, and whether it may.
		const LandCommand::Tag* T = Clip.TagOf(GeneralCarry);
		G.bCarry = T && bGround;
		G.Carried = GeneralCarry;
		if (G.bCarry)
		{
			G.bCarryOk = Clip.MayStand(GeneralCarry, X, Y);
			G.bCarryWoods = G.bCarryOk && WoodsAt(State->Rec.Ground, X, Y);
			G.Would = G.bCarryWoods ? "HIDE IN THE WOODS" : G.bCarryOk ? "" : "NOT ON YOUR GROUND";
			const double S1 = FMath::Sin(GeneralCarryFace), C1 = FMath::Cos(GeneralCarryFace);
			const double Hw = FMath::Max(T->W, 20.0) * 0.5, Hd = FMath::Max(T->D, 12.0) * 0.5;
			const double Corner[4][2] = { { -Hw, -Hd }, { Hw, -Hd }, { Hw, Hd }, { -Hw, Hd } };
			for (int32 K = 0; K < 4; ++K)
			{
				FVector2D P;
				G.bCarry = G.bCarry && PixelOf(X + Corner[K][0] * C1 + Corner[K][1] * S1, Y - Corner[K][0] * S1 + Corner[K][1] * C1, 0.5, P);
				G.Corners[K * 2] = P.X;
				G.Corners[K * 2 + 1] = P.Y;
			}
		}
	}
	else if (GeneralButton == 0 && G.Armed == "smoke")
	{
		// (smoke armed: the left drag draws the smoke's line on the ground, not a box)
		double X0 = 0.0, Y0 = 0.0;
		G.bSmoke = bGround && GroundUnder(GeneralDownAt, X0, Y0);
		G.SmokeEnds[0] = GeneralDownAt.X;
		G.SmokeEnds[1] = GeneralDownAt.Y;
		G.SmokeEnds[2] = Pixel.X;
		G.SmokeEnds[3] = Pixel.Y;
	}
	else if (GeneralButton == 0)
	{
		G.bBox = true;
		G.Box[0] = GeneralDownAt.X;
		G.Box[1] = GeneralDownAt.Y;
		G.Box[2] = Pixel.X;
		G.Box[3] = Pixel.Y;
	}
	else if (GeneralButton == 1 && !Clip.Chosen().empty())
	{
		// The line drawn on the ground, and where each block in hand would stand along it.
		double X0 = 0.0, Y0 = 0.0;
		G.bLine = bGround && GroundUnder(GeneralDownAt, X0, Y0);
		G.Ghosts.clear();
		if (G.bLine)
		{
			G.LineEnds[0] = GeneralDownAt.X;
			G.LineEnds[1] = GeneralDownAt.Y;
			G.LineEnds[2] = Pixel.X;
			G.LineEnds[3] = Pixel.Y;
			for (const LandCommand::Ghost& One : Clip.LineUp(X0, Y0, X, Y))
			{
				FVector2D P;
				if (PixelOf(One.X, One.Y, 0.5, P))
				{
					G.Ghosts.push_back({ P.X, P.Y });
				}
			}
		}
	}
}

void ALandBattle::GeneralRelease(const FVector2D& Pixel, int32 Button, bool bShift, int32 OnTag)
{
	if (!State.IsValid() || !State->General.bOn || State->General.bFilm || Button != GeneralButton)
	{
		return;
	}
	FLandBattleState& S = *State;
	FLandGeneralShown& G = S.General;
	LandCommand::General& Clip = G.Clipboard;
	const bool bMoved = bGeneralMoved;
	const FVector2D DownAt = GeneralDownAt;
	const int32 Carry = GeneralCarry;
	GeneralButton = -1;
	bGeneralMoved = false;
	GeneralCarry = -1;
	const bool bBox = G.bBox, bLine = G.bLine, bSmokeLine = G.bSmoke;
	G.bBox = G.bLine = G.bCarry = G.bSmoke = false;
	G.Ghosts.clear();
	double X = 0.0, Y = 0.0;
	const bool bGround = GroundUnder(Pixel, X, Y);

	// A block carried in the setup step and set down: placed where it may stand (the rules have the last word).
	if (Carry >= 0)
	{
		if (!bMoved)
		{
			Clip.Pick(Carry, bShift);
		}
		else if (bGround && Clip.MayStand(Carry, X, Y))
		{
			PlaceLive(Carry, X, Y, GeneralCarryFace);
		}
		else
		{
			S.Note = "IT MUST STAND ON YOUR OWN GROUND";
			S.NoteAge = 0.0;
		}
		return;
	}
	// Smoke armed, a line dragged: those in hand with smoke lay it along the line.
	if (Button == 0 && bMoved && bSmokeLine)
	{
		double X0 = 0.0, Y0 = 0.0;
		if (bGround && GroundUnder(DownAt, X0, Y0))
		{
			GiveSmoke(X0, Y0, X, Y);
		}
		return;
	}
	// A box round several: his blocks whose stalks stand inside it.
	if (Button == 0 && bMoved && bBox)
	{
		const double L = FMath::Min(DownAt.X, Pixel.X), R = FMath::Max(DownAt.X, Pixel.X);
		const double T = FMath::Min(DownAt.Y, Pixel.Y), B = FMath::Max(DownAt.Y, Pixel.Y);
		std::vector<int> Inside;
		for (const FLandGeneralShown::Mark& M : G.Marks)
		{
			if (M.FootX >= L && M.FootX <= R && M.FootY >= T && M.FootY <= B)
			{
				Inside.push_back(M.Block);
			}
		}
		Clip.Choose(Inside, bShift);
		return;
	}
	// A line drawn with the right button: the blocks in hand spread along it.
	if (Button == 1 && bMoved)
	{
		double X0 = 0.0, Y0 = 0.0;
		if (bLine && bGround && GroundUnder(DownAt, X0, Y0))
		{
			TArray<FVector2D> Points;
			for (const LandCommand::Ghost& One : Clip.LineUp(X0, Y0, X, Y))
			{
				Points.Add(FVector2D(One.X, One.Y));
			}
			GiveGeneral(WideOf(Clip.Line(X0, Y0, X, Y)), TEXT("line"), -1, Points, (X0 + X) * 0.5, (Y0 + Y) * 0.5);
		}
		return;
	}
	if (bMoved)
	{
		return;
	}

	// A click. The left button on one of his blocks (or its tag) picks it up; otherwise, with blocks in hand, either button
	// gives the order: an enemy there is attacked (guns alone, or "fire on" armed, fire on it), open ground is gone to.
	const double Reach = bGround ? ReachAt(Pixel) : 0.0;
	const LandCommand::Tag* On = OnTag >= 0 ? Clip.TagOf(OnTag) : nullptr;
	const int32 Mine = On ? (On->bOrderable ? OnTag : -1) : bGround ? Clip.MineAt(X, Y, Reach * 0.5) : -1;
	if (Button == 0 && Mine >= 0)
	{
		Clip.Pick(Mine, bShift);
		return;
	}
	// (his despatch rider: a click reads out what his corps coming up brings, and when; Brian, 22 November 12:48Z)
	const int Rider = Button == 0 && G.Armed.empty() && bGround ? Clip.RiderAt(X, Y, FMath::Max(Reach * 0.6, 15.0)) : -1;
	if (Rider >= 0)
	{
		G.RiderRead = G.RiderRead == Clip.Riders()[static_cast<size_t>(Rider)].Corps ? -1 : Clip.Riders()[static_cast<size_t>(Rider)].Corps;
		G.RiderReadAt = RealT;
		return;
	}
	const std::vector<int> Hand = Clip.Chosen();
	if (Hand.empty() || Clip.Deploying())
	{
		// (nothing to order: a double click on the ground glides the view there)
		const bool bTwiceHere = RealT - GeneralClickAt < TwiceSeconds && (Pixel - GeneralClickWhere).Size() < TwicePixels;
		if (bGround && bTwiceHere)
		{
			S.GeneralIn.bGoTo = true;
			S.GeneralIn.GoTo = LandPlay::Vec(X, Y, 0.0);
			S.GeneralIn.bFollow = false;
			GeneralClickAt = -10.0;
		}
		else
		{
			GeneralClickAt = RealT;
			GeneralClickWhere = Pixel;
		}
		return;
	}
	// (the cutters armed: the tank with them goes to the hedge nearest the click and punches a gap in it; Brian, 22
	// November 14:19Z)
	if (G.Armed == "gap" && bGround)
	{
		double Gx = 0.0, Gy = 0.0;
		const int Cutter = Clip.Cutter();
		if (Cutter < 0 || !HedgeNear(X, Y, FMath::Max(Reach, 30.0), Gx, Gy))
		{
			S.Note = Cutter < 0 ? "NO TANK WITH HEDGE CUTTERS IN HAND" : "CLICK ON A HEDGE TO CUT A GAP IN IT";
			S.NoteAge = 0.0;
			return;
		}
		GiveGeneral(WideOf(Clip.Cut(Gx, Gy)), TEXT("gap"), -1, { FVector2D(Gx, Gy) }, Gx, Gy, { Cutter });
		G.Armed.clear();
		G.bGapAt = false;
		return;
	}
	// (smoke armed, a click: a screen across the front at the point, square to the way those laying it look at it)
	if (G.Armed == "smoke" && bGround)
	{
		double Cx = 0.0, Cy = 0.0;
		const std::vector<int> Layers = Clip.Smokers();
		for (int Id : Layers)
		{
			const LandCommand::Tag* T = Clip.TagOf(Id);
			Cx += T ? T->X : X;
			Cy += T ? T->Y : Y;
		}
		if (!Layers.empty())
		{
			Cx /= static_cast<double>(Layers.size());
			Cy /= static_cast<double>(Layers.size());
		}
		double Dx = X - Cx, Dy = Y - Cy;
		const double Long = FMath::Sqrt(Dx * Dx + Dy * Dy);
		if (Long < 1.0)
		{
			// (on top of them: across the field, facing the enemy's side)
			Dx = 0.0;
			Dy = Clip.Side() == 1 ? -1.0 : 1.0;
		}
		else
		{
			Dx /= Long;
			Dy /= Long;
		}
		const double Half = SmokeHalfWidth;
		GiveSmoke(X + Dy * Half, Y - Dx * Half, X - Dy * Half, Y + Dx * Half);
		return;
	}
	const int32 Foe = On ? (!On->bMine ? OnTag : -1) : bGround ? Clip.EnemyAt(X, Y, Reach) : -1;
	if (Foe >= 0)
	{
		const LandCommand::Tag* Target = Clip.TagOf(Foe);
		const bool bGuns = std::all_of(Hand.begin(), Hand.end(), [&Clip](int Id) { const LandCommand::Tag* T = Clip.TagOf(Id); return T && T->Kind == "art"; });
		const bool bFire = bGuns || G.Armed == "fire";
		const std::string Command = bFire ? Clip.FireOn(Foe) : Clip.Attack(Foe);
		if (Command.empty())
		{
			S.Note = "ONLY GUNS FIRE ON IT FROM WHERE THEY STAND";
			S.NoteAge = 0.0;
			return;
		}
		GiveGeneral(WideOf(Command), bFire ? TEXT("fire") : TEXT("attack"), Foe, TArray<FVector2D>(), Target ? Target->X : X, Target ? Target->Y : Y);
		G.Armed.clear();
		return;
	}
	if (!bGround)
	{
		return;
	}
	// (FIRE ON armed, a click on the ground: they stand and fire into the cover there, to flush out what hides in it; the
	// historian's check, 22 November)
	if (G.Armed == "fire")
	{
		GiveGeneral(WideOf(Clip.FireInto(X, Y)), TEXT("fire"), -1, TArray<FVector2D>(), X, Y);
		G.Armed.clear();
		return;
	}
	// (a second click where the last order went sends them at the double; or "at the double" armed)
	const bool bTwice = RealT - GeneralClickAt < TwiceSeconds && (Pixel - GeneralClickWhere).Size() < TwicePixels;
	const bool bRun = G.Armed == "double" || bTwice;
	// (each block to the point and its place in the group, as the rules send them: the group keeps its shape)
	TArray<FVector2D> Points;
	double Cx = 0.0, Cy = 0.0;
	for (int Id : Hand)
	{
		const LandCommand::Tag* T = Clip.TagOf(Id);
		Cx += T ? T->X : X;
		Cy += T ? T->Y : Y;
	}
	Cx /= static_cast<double>(Hand.size());
	Cy /= static_cast<double>(Hand.size());
	for (int Id : Hand)
	{
		const LandCommand::Tag* T = Clip.TagOf(Id);
		Points.Add(Hand.size() > 1 && T ? FVector2D(X + T->X - Cx, Y + T->Y - Cy) : FVector2D(X, Y));
	}
	GiveGeneral(WideOf(Clip.Move(X, Y, bRun)), TEXT("move"), -1, Points, X, Y);
	GeneralClickAt = bTwice ? -10.0 : RealT;
	GeneralClickWhere = Pixel;
	G.Armed.clear();
}

void ALandBattle::GiveGeneral(const FString& Command, const FString& Kind, int32 Target, const TArray<FVector2D>& Points, double PingX, double PingY, const TArray<int32>& Only)
{
	if (Command.IsEmpty() || !State.IsValid() || !Live.IsValid())
	{
		return;
	}
	FLandBattleState& S = *State;
	FLandGeneralShown& G = S.General;
	std::vector<int> Ids = G.Clipboard.Chosen();
	if (Only.Num() > 0)
	{
		Ids.assign(Only.GetData(), Only.GetData() + Only.Num());
	}
	if (Kind == TEXT("staff"))
	{
		Ids.erase(std::remove_if(Ids.begin(), Ids.end(), [&G](int Id) { const LandCommand::Tag* T = G.Clipboard.TagOf(Id); return !T || !T->bTaken; }), Ids.end());
	}
	OrderLive(Command);
	// (while the film has the camera, its next cut goes to the men he has just ordered, as a war film cuts from the radio
	// call to the men it sends: the camera thread, 22 November; never from his own view, or it would fire on the film's
	// return)
	if (G.bFilm && !Ids.empty())
	{
		S.Eye.Lean(Ids.front());
	}
	std::vector<std::pair<double, double>> At;
	for (const FVector2D& P : Points)
	{
		At.push_back({ P.X, P.Y });
	}
	// (shown on its tags until the screen reaches the views that carry it: the rules' time now, a piece and a view on)
	G.Clipboard.Sent(Ids, Kind == TEXT("fire") ? std::string("attack") : Kind == TEXT("gap") ? std::string("move") : NarrowOf(Kind), At, Target, S.Rec.Fought + 2.0);
	G.PingFieldX = PingX;
	G.PingFieldY = PingY;
	G.PingAt = RealT;
	G.bPingAttack = Kind == TEXT("attack") || Kind == TEXT("fire");
}

bool ALandBattle::HedgeNear(double X, double Y, double Reach, double& OutX, double& OutY) const
{
	if (!State.IsValid() || !State->Field)
	{
		return false;
	}
	std::vector<const std::vector<std::pair<double, double>>*> Lines;
	for (const LandField::HedgeLine& H : State->Field->Hedges())
	{
		// (what cutters go through: a hedgerow, a lane's bank, a garden's hedge; not a stone wall, not a wood's edge)
		if (H.Kind == 0 || H.Kind == 1 || H.Kind == 3)
		{
			Lines.push_back(&H.Points);
		}
	}
	return LandCommand::NearestOnLines(Lines, X, Y, Reach, OutX, OutY);
}

void ALandBattle::GiveSmoke(double X0, double Y0, double X1, double Y1)
{
	if (!State.IsValid())
	{
		return;
	}
	FLandGeneralShown& G = State->General;
	const std::vector<int> Layers = G.Clipboard.Smokers();
	if (Layers.empty())
	{
		State->Note = "NO SMOKE LEFT WITH THOSE IN HAND";
		State->NoteAge = 0.0;
		G.Armed.clear();
		return;
	}
	TArray<int32> Only;
	for (int Id : Layers)
	{
		Only.Add(Id);
	}
	// (they lay it from where they stand: no route, the ring where the screen's middle falls)
	GiveGeneral(WideOf(G.Clipboard.SmokeLine(X0, Y0, X1, Y1)), TEXT("smoke"), -1, TArray<FVector2D>(), (X0 + X1) * 0.5, (Y0 + Y1) * 0.5, Only);
	G.Armed.clear();
}

// ---- His view ---------------------------------------------------------------------------------------------------

void ALandBattle::GeneralView(double Real)
{
	if (!State.IsValid())
	{
		return;
	}
	FLandBattleState& S = *State;
	FLandGeneralShown& G = S.General;
	if (!S.Rec.bLive || S.Rec.Me < 0)
	{
		return;
	}
	if (!S.bGeneralEye)
	{
		// Begun once his blocks are on the clipboard: over his ground (or his blocks, the setup step skipped), facing the
		// enemy's side, high enough to see the whole of his line.
		const std::vector<LandCommand::Tag>& Tags = G.Clipboard.Tags();
		if (Tags.empty())
		{
			return;
		}
		double X = 0.0, Y = 0.0;
		int Mine = 0;
		if (G.Clipboard.Deploying())
		{
			const double* Room = G.Clipboard.Room();
			X = 0.5 * (Room[0] + Room[2]);
			Y = 0.5 * (Room[1] + Room[3]);
			Mine = 1;
		}
		else
		{
			for (const LandCommand::Tag& T : Tags)
			{
				if (T.bMine && T.Says != LandCommand::Chip::Gone)
				{
					X += T.X;
					Y += T.Y;
					++Mine;
				}
			}
		}
		if (Mine == 0)
		{
			X = S.Rec.Ground.W * 0.5;
			Y = S.Rec.Ground.H * 0.5;
			Mine = 1;
		}
		const double Heading = G.Clipboard.Side() == 1 ? UE_DOUBLE_PI : 0.0;
		LandFell::Phase("general: his eye begun");
		S.bGeneralEye = S.GeneralEye.Begin(S.Rec, S.EyeWorld, LandPlay::Vec(X / Mine, Y / Mine, 0.0), Heading, 250.0);
		if (!S.bGeneralEye)
		{
			return;
		}
	}
	LandGeneralEye::Input& In = S.GeneralIn;
	// (each check's line is written below, once his eye has taken the step: before it, S.Pose is the film's this frame
	// and the kept views are as they were before the step)
	int32 TopLogged = -1, ViewsLogged = -1;
	// (the Mac's check of his top view, -TheaterLandTopViewAt=<battle seconds>: pressed there, a still 4 s on (into the
	// -TheaterLandScript folder, if one is given), pressed again 4 s later and a still of the view back as it was 4 s
	// after that; each step's tilt and height in the log)
	if (TopCheckAt < -1.0)
	{
		double At = -1.0;
		TopCheckAt = FParse::Value(FCommandLine::Get(), TEXT("TheaterLandTopViewAt="), At) ? At : -1.0;
	}
	if (TopCheckAt >= 0.0 && TopCheckStep < 4 && !G.bFilm && S.Battle >= TopCheckAt + 4.0 * TopCheckStep)
	{
		if (TopCheckStep == 0 || TopCheckStep == 2)
		{
			In.bTopView = true;
		}
		else
		{
			ScriptStill(TopCheckStep == 1 ? TEXT("top-view") : TEXT("top-view-back"));
		}
		TopLogged = TopCheckStep++;
	}
	// (the Mac's check of his kept views, the spotter's mark and the film in a corner, release 2,
	// -TheaterLandViewsAt=<battle seconds>: a unit taken in hand there and the view glided to it; 4 s on that view kept
	// as view 1, then the top view pressed; 4 s on a still of it (the rings, the chosen unit's three, the film's window
	// where their switches are on) and that view kept as view 2; then view 1 called, a still 6 s on, view 2 called, a
	// still 6 s on. Each step's view in the log; the stills into the -TheaterLandScript folder, if one is given, and
	// then never before that walk's own steps are done, so its three minutes at the battle's pace are read with these on)
	if (ViewsCheckAt < -1.0)
	{
		double At = -1.0;
		ViewsCheckAt = FParse::Value(FCommandLine::Get(), TEXT("TheaterLandViewsAt="), At) ? At : -1.0;
	}
	static const double ViewsCheckSteps[] = { 0.0, 4.0, 5.0, 9.0, 10.0, 16.0, 17.0, 23.0 };
	if (ViewsCheckAt >= 0.0 && ViewsCheckStep < 8 && !G.bFilm && S.bGeneralEye && !G.Clipboard.Deploying() && (ScriptFolder.IsEmpty() || ScriptStep >= 60)
		&& S.Battle >= ViewsCheckAt + ViewsCheckSteps[ViewsCheckStep])
	{
		if (ViewsCheckStep == 0)
		{
			// (the later steps count from when the first was let go)
			ViewsCheckAt = S.Battle;
		}
		switch (ViewsCheckStep)
		{
		case 0: GeneralDo(TEXT("next")); break;
		case 1: GeneralDo(TEXT("keepview:0")); break;
		case 2: In.bTopView = !S.GeneralEye.TopView(); break;
		case 3: ScriptStill(TEXT("views-top-rings")); GeneralDo(TEXT("keepview:1")); break;
		case 4: GeneralDo(TEXT("view:0")); break;
		case 5: ScriptStill(TEXT("views-back-1")); break;
		case 6: GeneralDo(TEXT("view:1")); break;
		default: ScriptStill(TEXT("views-back-2")); break;
		}
		ViewsLogged = ViewsCheckStep++;
	}
	In.Turn += S.GeneralTurnHeld * 75.0 * Real;
	// (Shift with W or S held: his own tilt, 50 degrees a second)
	In.Tilt += S.GeneralTiltHeld * 50.0 * Real;
	if (In.PanRight != 0.0 || In.PanAhead != 0.0 || In.EdgeRight != 0.0 || In.EdgeAhead != 0.0 || In.SlideRight != 0.0 || In.SlideAhead != 0.0 || In.bGoTo)
	{
		In.bFollow = false;
	}
	if (!G.bFilm)
	{
		LandFell::Phase("general: his eye moved", In.bGoTo ? 1 : 0);
		S.Pose = S.GeneralEye.Update(In, Real, S.Battle);
	}
	if (TopLogged >= 0)
	{
		UE_LOG(LogTheater, Display, TEXT("Land battle (top view check): step %d done at %.1f s of battle: tilt %.1f degrees, top view %s, own tilt %s, the eye %.0f m over what it looks at"),
			TopLogged, S.Battle, S.GeneralEye.TiltDegrees(), S.GeneralEye.TopView() ? TEXT("on") : TEXT("off"), S.GeneralEye.OwnTilt() ? TEXT("on") : TEXT("off"),
			S.GeneralEye.Current().Eye.Z - S.GeneralEye.Current().Look.Z);
	}
	if (ViewsLogged >= 0)
	{
		// (his own view, whoever has the screen: the general's eye's pose, not S.Pose; the rings are last frame's, laid
		// after this; the window is this frame's only once its picture is taken, later in the tick)
		const LandEye::Pose& Own = S.GeneralEye.Current();
		// (rings, not their strokes: a ring is laid as one stroke or as many, by its dashes and by the screen's edge;
		// a ring's strokes follow one another)
		int32 RingsLaid = 0;
		for (size_t Stroke = 0; Stroke < G.Sights.size(); ++Stroke)
		{
			if (Stroke == 0 || G.Sights[Stroke].Block != G.Sights[Stroke - 1].Block || G.Sights[Stroke].Kind != G.Sights[Stroke - 1].Kind)
			{
				++RingsLaid;
			}
		}
		UE_LOG(LogTheater, Display, TEXT("Land battle (views check): step %d done at %.1f s of battle: at view %d, flying to %d, turn and zoom %s, kept %d%d%d%d, tilt %.1f degrees, top view %s, looking at %.0f, %.0f from %.0f m over it; rings last frame %d, the film's window last frame %s"),
			ViewsLogged, S.Battle, S.GeneralEye.ViewAt() + 1, S.GeneralEye.FlyingTo() + 1, S.GeneralEye.Flying() ? TEXT("held") : TEXT("free"),
			S.GeneralEye.ViewKept(0) ? 1 : 0, S.GeneralEye.ViewKept(1) ? 1 : 0, S.GeneralEye.ViewKept(2) ? 1 : 0, S.GeneralEye.ViewKept(3) ? 1 : 0,
			S.GeneralEye.TiltDegrees(), S.GeneralEye.TopView() ? TEXT("on") : TEXT("off"), Own.Look.X, Own.Look.Y, Own.Eye.Z - Own.Look.Z,
			RingsLaid, bCornerShown ? TEXT("shown") : TEXT("not shown"));
	}
	if (S.bEyeSwitched)
	{
		S.Pose.bCut = true;
		S.bEyeSwitched = false;
	}
	// (this frame's moves spent; the keys held stay held)
	In.SlideRight = In.SlideAhead = In.Zoom = In.Turn = In.Tilt = 0.0;
	In.bGoTo = false;
	In.bTopView = false;
	In.GoBlock = -1;
	In.KeepView = In.CallView = -1;
}

// ---- The film in a corner (release 2) ----------------------------------------------------------------------------

void ALandBattle::FilmCorner()
{
	// Brian, 24 November 20:38Z: while he commands from his own view, the film director's shot in a small window. The
	// film's eye runs every frame whoever has the camera, so its pose is there to take: a second picture of the field
	// is drawn small through it every few frames, and the general's screen shows it under the news at its left.
	// Here, once the lens is aimed, the window's own lens is set up and aimed for this frame; its picture is taken
	// by FilmCornerTake once the men and the vehicles are drawn where this frame has them.
	if (CornerOn < 0)
	{
		CornerOn = FParse::Param(FCommandLine::Get(), TEXT("TheaterLandCorner")) ? 1 : 0;
		int32 Wide = 480, Every = 2;
		float TrueWithin = 40.0f;
		FParse::Value(FCommandLine::Get(), TEXT("TheaterLandCornerWide="), Wide);
		FParse::Value(FCommandLine::Get(), TEXT("TheaterLandCornerEvery="), Every);
		FParse::Value(FCommandLine::Get(), TEXT("TheaterLandCornerTrueWithin="), TrueWithin);
		CornerWide = FMath::Clamp(Wide, 160, 1280);
		CornerEvery = FMath::Clamp(Every, 1, 8);
		CornerTrueWithin = FMath::Clamp(static_cast<double>(TrueWithin), 0.0, 5000.0);
		if (CornerOn == 1)
		{
			UE_LOG(LogTheater, Display, TEXT("Land battle (film in a corner): on, %d by %d, a picture every %d frames%s; men in its picture within %.0f m of its lens drawn their own size"), CornerWide, CornerWide * 9 / 16, CornerEvery,
				FParse::Param(FCommandLine::Get(), TEXT("TheaterLandCornerNoShadows")) ? TEXT(", asked without shadows") : TEXT(""), CornerTrueWithin);
		}
	}
	const bool bLensWas = bCornerLens;
	bCornerLens = false;
	bCornerShown = false;
	if (CornerOn != 1 || !State.IsValid() || !Eye)
	{
		return;
	}
	FLandBattleState& S = *State;
	const FLandGeneralShown& G = S.General;
	// (only while he commands from his own view: the film full on the screen, a card up, or the night's patrol being
	// shown, needs no second picture and pays for none)
	if (!G.bOn || G.bFilm || S.bCard || !S.bGeneralEye || S.NightNow.bNight)
	{
		return;
	}
	const LandEye::Pose& P = S.FilmPose;
	const FVector From = World(P.Eye.X, P.Eye.Y, P.Eye.Z);
	const FVector To = World(P.Look.X, P.Look.Y, P.Look.Z);
	if ((To - From).SizeSquared() < 1.0)
	{
		// (no shot of the film's yet)
		return;
	}
	if (!CornerLens)
	{
		CornerFilm = NewObject<UTextureRenderTarget2D>(this, FName(TEXT("LandCornerFilm")));
		CornerFilm->ClearColor = FLinearColor(0.02f, 0.02f, 0.02f, 1.0f);
		// (the same making as the campaign's unit pictures, which the art director has passed on the Mac)
		CornerFilm->InitCustomFormat(CornerWide, CornerWide * 9 / 16, PF_B8G8R8A8, false);
		CornerFilm->UpdateResourceImmediate(true);
		CornerLens = NewObject<USceneCaptureComponent2D>(this, FName(TEXT("LandCornerLens")));
		CornerLens->SetupAttachment(Root);
		CornerLens->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
		CornerLens->bCaptureEveryFrame = false;
		CornerLens->bCaptureOnMovement = false;
		// (kept from one picture to the next, or every picture is taken at one fixed exposure whatever it is told)
		CornerLens->bAlwaysPersistRenderingState = true;
		// (nothing that needs several frames to settle: the film cuts, and the window is drawn only every few frames)
		// (all of it sharp: the grade below is the main lens's, whose focus is set for his own view, not this one)
		// (given as settings, not set on ShowFlags: registering reads ShowFlags afresh from the class's own and these;
		// the pre-check's mend, 24 November)
		TArray<FEngineShowFlagsSetting> CornerOff;
		for (const TCHAR* Flag : { TEXT("TemporalAA"), TEXT("MotionBlur"), TEXT("DepthOfField") })
		{
			FEngineShowFlagsSetting& One = CornerOff.AddDefaulted_GetRef();
			One.ShowFlagName = Flag;
			One.Enabled = false;
		}
		if (FParse::Param(FCommandLine::Get(), TEXT("TheaterLandCornerNoShadows")))
		{
			FEngineShowFlagsSetting& One = CornerOff.AddDefaulted_GetRef();
			One.ShowFlagName = TEXT("DynamicShadows");
			One.Enabled = false;
		}
		CornerLens->SetShowFlagSettings(CornerOff);
		CornerLens->TextureTarget = CornerFilm;
		CornerLens->RegisterComponent();
		// (and set on the registered lens itself as well: should the settings above not be taken, these stand until it
		// is next registered, which it is not)
		CornerLens->ShowFlags.SetTemporalAA(false);
		CornerLens->ShowFlags.SetMotionBlur(false);
		CornerLens->ShowFlags.SetDepthOfField(false);
		if (FParse::Param(FCommandLine::Get(), TEXT("TheaterLandCornerNoShadows")))
		{
			CornerLens->ShowFlags.SetDynamicShadows(false);
		}
		// (what the lens really has, said once: a trial with and without shadows must know which it ran)
		UE_LOG(LogTheater, Display, TEXT("Land battle (film in a corner): its lens draws shadows %d, depth of field %d, motion blur %d, temporal AA %d"),
			CornerLens->ShowFlags.DynamicShadows ? 1 : 0, CornerLens->ShowFlags.DepthOfField ? 1 : 0, CornerLens->ShowFlags.MotionBlur ? 1 : 0, CornerLens->ShowFlags.TemporalAA ? 1 : 0);
	}
	FRotator Facing = (To - From).Rotation();
	Facing.Roll = P.Roll;
	CornerLens->SetWorldLocationAndRotation(From, Facing);
	// (the pose's lens is the vertical one; the window is 16 by 9)
	const double Horizontal = 2.0 * FMath::RadiansToDegrees(FMath::Atan(FMath::Tan(FMath::DegreesToRadians(P.Fov * 0.5)) * 16.0 / 9.0));
	CornerLens->FOVAngle = static_cast<float>(FMath::Clamp(Horizontal, 5.0, 120.0));
	// (the battle's own grade and exposure, as its eye has them this frame)
	const UCameraComponent* Lens = Eye->GetCameraComponent();
	CornerLens->PostProcessSettings = Lens->PostProcessSettings;
	CornerLens->PostProcessBlendWeight = Lens->PostProcessBlendWeight;
	// (back after the film had the screen, a card or the night: the picture it holds is from before, so a new one is
	// taken this frame, wherever the count of frames stood)
	if (!bLensWas)
	{
		CornerFrame = 0;
	}
	bCornerLens = true;
}

void ALandBattle::FilmCornerTake()
{
	// (the window's picture, every few frames while its lens is up: taken once this frame's men and vehicles stand
	// where it has them, those in the film's picture among them (DrawMen), so the film's first frame after a cut is
	// not of men posed for the shot before)
	if (!bCornerLens || !CornerLens)
	{
		return;
	}
	if (CornerFrame++ % CornerEvery == 0)
	{
		// (a picture asked for first sends the frame's own end-of-frame updates, every man, vehicle and effect just
		// set: sent here and timed apart, so the window's figure is the window's alone. The frame pays that sending
		// with or without the window; what is set after this is sent at the frame's end as ever.)
		const double Sent = FPlatformTime::Seconds();
		if (UWorld* const Around = GetWorld())
		{
			Around->SendAllEndOfFrameUpdates();
		}
		const double Mark = FPlatformTime::Seconds();
		CornerLens->CaptureScene();
		CornerMs += (FPlatformTime::Seconds() - Mark) * 1000.0;
		CornerSendMs += (Mark - Sent) * 1000.0;
		++CornerTaken;
		// (its cost on the game's side every few hundred pictures, for the Mac's reading; the card's side shows in fps)
		if (CornerTaken % 300 == 0)
		{
			UE_LOG(LogTheater, Display, TEXT("Land battle (film in a corner): %d pictures, %.2f ms each to ask for (the game's side), after %.2f ms each of the frame's own end-of-frame sending, which the frame pays with or without the window"), CornerTaken, CornerMs / 300.0, CornerSendMs / 300.0);
			CornerMs = 0.0;
			CornerSendMs = 0.0;
		}
	}
	bCornerShown = CornerTaken > 0;
}

// ---- His words --------------------------------------------------------------------------------------------------

void ALandBattle::GeneralDo(const FString& Word)
{
	if (!State.IsValid() || !State->General.bOn)
	{
		return;
	}
	FLandBattleState& S = *State;
	FLandGeneralShown& G = S.General;
	LandCommand::General& Clip = G.Clipboard;
	const bool bHand = !Clip.Chosen().empty();
	auto Say = [&S](const char* Words)
	{
		S.Note = Words;
		S.NoteAge = 0.0;
	};
	if (Word == TEXT("film"))
	{
		S.bEyeSwitched = S.bEyeSwitched || !G.bFilm;
		G.bFilm = true;
		// (the film as it chooses: any ride let go)
		S.Eye.Ride(-1);
		Say("THE FILM HAS THE CAMERA: ESC BRINGS THE MAP BACK");
	}
	else if (Word == TEXT("map"))
	{
		S.bEyeSwitched = S.bEyeSwitched || G.bFilm;
		G.bFilm = false;
		S.Eye.Ride(-1);
	}
	else if (Word == TEXT("topview"))
	{
		// (O or TOP VIEW: his view swings to look down on the field from about 100 m up; again, back as it was)
		S.GeneralIn.bTopView = true;
		if (!S.GeneralEye.TopView())
		{
			Say("TOP VIEW: O AGAIN GOES BACK. SHIFT+W OR S, OR OPTION AND TWO FINGERS, TILTS THE VIEW");
		}
	}
	else if (Word.StartsWith(TEXT("view:")) || Word.StartsWith(TEXT("keepview:")))
	{
		// (his kept views, release 2: F1 to F4, Option with 1 to 4, or a chip. A kept one is flown to; one never kept,
		// or any with Shift, is kept: where his own hand's last move brings the view to rest, and as it stands for
		// what is not his own hand's, LandGeneralEye's KeepView)
		const bool bKeep = Word.StartsWith(TEXT("keepview:"));
		const int32 Number = FCString::Atoi(*Word.Mid(bKeep ? 9 : 5));
		if (Number >= 0 && Number < LandGeneralEye::ViewCount && S.bGeneralEye && !G.bFilm)
		{
			const bool bKeeps = bKeep || !S.GeneralEye.ViewKept(Number);
			(bKeeps ? S.GeneralIn.KeepView : S.GeneralIn.CallView) = Number;
			const std::string Digit = std::to_string(Number + 1);
			S.Note = bKeeps ? "VIEW " + Digit + " KEPT HERE: F" + Digit + ", OR OPTION AND " + Digit + ", FLIES BACK TO IT" : "VIEW " + Digit + ": SHIFT WITH IT KEEPS THE VIEW YOU HAVE NOW INSTEAD";
			S.NoteAge = 0.0;
		}
	}
	else if (Word.StartsWith(TEXT("follow:")))
	{
		// (a unit's card clicked twice: the view glides to it and keeps it in the middle till he moves the view)
		S.GeneralIn.GoBlock = FCString::Atoi(*Word.Mid(7));
		S.GeneralIn.bFollow = true;
	}
	else if (Word == TEXT("begin"))
	{
		BeginFight();
	}
	else if (Word == TEXT("clear"))
	{
		G.Armed.clear();
		Clip.Clear();
		G.bBox = G.bLine = G.bCarry = G.bSmoke = G.bGapAt = false;
		GeneralButton = -1;
		GeneralCarry = -1;
	}
	else if (Word == TEXT("letgo"))
	{
		// (the pointer lost mid-drag: nothing given, nothing carried)
		G.bBox = G.bLine = G.bCarry = G.bSmoke = false;
		G.Ghosts.clear();
		GeneralButton = -1;
		bGeneralMoved = false;
		GeneralCarry = -1;
	}
	else if (Word == TEXT("next") || Word == TEXT("back"))
	{
		// (Tab: his next unit in hand, and the view glides to it)
		const int Picked = Clip.Next(Word == TEXT("back"));
		if (Picked >= 0)
		{
			S.GeneralIn.GoBlock = Picked;
			S.GeneralIn.bFollow = false;
		}
	}
	// (ALL FALL BACK is for his whole army, so it is answered before "pick up a unit first": with nothing in hand it was
	// refused, and the button for the whole army did nothing until a unit was picked up. The R2 Mac pass of 25 November 2026.)
	else if (Word == TEXT("allback"))
	{
		// ALL FALL BACK (release 1.5; Brian, 23 November 14:47Z): asked once, on the screen, before it is given.
		if (!G.bAllBack && !Clip.Deploying())
		{
			G.bAllBackAsk = true;
		}
	}
	else if (Word == TEXT("allback:no"))
	{
		G.bAllBackAsk = false;
	}
	else if (Word == TEXT("allback:yes"))
	{
		// His whole army goes back behind the ground it started from; the rules end the battle as his retreat when the last
		// unit is out. Nothing is left in his hand: the army takes no new orders after it (a refusal gives the button back).
		G.bAllBackAsk = false;
		if (!G.bAllBack && !Clip.Deploying())
		{
			G.bAllBack = true;
			G.Armed.clear();
			Clip.Clear();
			G.bBox = G.bLine = G.bCarry = G.bSmoke = G.bGapAt = false;
			OrderLive(TEXT("{\"do\":\"land:fallBack\"}"));
			Say("YOUR WHOLE ARMY FALLS BACK: THE BATTLE ENDS WHEN THE LAST UNIT IS BEHIND THE GROUND IT STARTED FROM");
		}
	}
	else if (!bHand)
	{
		Say("PICK UP A UNIT FIRST: CLICK ONE OF YOURS");
	}
	else if (Word == TEXT("double") || Word == TEXT("fire"))
	{
		// (any unit may fire into cover; on an enemy unit, only guns fire from where they stand)
		G.Armed = G.Armed == NarrowOf(Word) ? std::string() : NarrowOf(Word);
	}
	else if (Word == TEXT("gap"))
	{
		if (Clip.Cutter() < 0)
		{
			Say("NO TANK WITH HEDGE CUTTERS IN HAND");
		}
		else
		{
			G.Armed = G.Armed == "gap" ? std::string() : std::string("gap");
			Say(G.Armed.empty() ? "" : "CLICK ON A HEDGE: THE TANK WITH CUTTERS PUNCHES A GAP THROUGH IT");
		}
	}
	else if (Word == TEXT("smoke"))
	{
		if (Clip.Smokers().empty())
		{
			Say("NO SMOKE LEFT WITH THOSE IN HAND");
		}
		else
		{
			G.Armed = G.Armed == "smoke" ? std::string() : std::string("smoke");
			Say(G.Armed.empty() ? "" : "CLICK WHERE THE SMOKE GOES, OR DRAG A LINE FOR IT");
		}
	}
	else if (Word == TEXT("wait"))
	{
		// (hold until relieved, release 2: those in hand on foot dig in and wait for a corps coming up from next door; the
		// rules take one a corps and say why for the rest. The warning in the UI director's words, as the order card has it)
		const std::vector<int> Foot = Clip.Waiters();
		if (Foot.empty())
		{
			Say("ONLY MEN ON FOOT DIG IN TO WAIT FOR HELP");
		}
		else
		{
			TArray<int32> Only;
			for (int Id : Foot)
			{
				Only.Add(Id);
			}
			GiveGeneral(WideOf(Clip.Wait()), TEXT("hold"), -1, TArray<FVector2D>(), 0.0, 0.0, Only);
			G.PingAt = -10.0;
			Say("THEY DIG IN TO WAIT FOR HELP. IF HELP HAS NOT COME BY ITS TIME, THEY BREAK SOONER");
		}
	}
	else if (Word == TEXT("patrol"))
	{
		// (night patrol, release 2: while he places his army, one of his blocks on foot goes out the night before; the
		// rules take one a night and say why where they refuse)
		const std::vector<int> Foot = Clip.Waiters();
		if (Foot.empty())
		{
			Say("ONLY MEN ON FOOT GO OUT ON PATROL");
		}
		else
		{
			OrderLive(FString::Printf(TEXT("{\"do\":\"land:patrol\",\"unit\":%d}"), Foot.front()));
			Say("THEIR PATROL GOES OUT TONIGHT, BEFORE THE BATTLE: IT MAY FIND A HIDDEN POST, OR BE CAUGHT");
		}
	}
	else if (Word == TEXT("hold"))
	{
		GiveGeneral(WideOf(Clip.Hold()), TEXT("hold"), -1, TArray<FVector2D>(), 0.0, 0.0);
		G.PingAt = -10.0;
	}
	else if (Word == TEXT("fallback"))
	{
		// (back toward his own edge of the field)
		TArray<FVector2D> Points;
		for (int Id : Clip.Chosen())
		{
			const LandCommand::Tag* T = Clip.TagOf(Id);
			Points.Add(FVector2D(T ? T->X : 0.0, Clip.Side() == 0 ? 0.0 : S.Rec.Ground.H));
		}
		GiveGeneral(WideOf(Clip.FallBack()), TEXT("withdraw"), -1, Points, 0.0, 0.0);
		G.PingAt = -10.0;
	}
	else if (Word == TEXT("ride"))
	{
		// Ride with this unit (Brian's yes, 22 November): the film takes the camera and stays with the unit in his hand
		// (the first, if he holds several), cutting among its men or tanks; it lets go of itself when none of them is
		// left standing, and ESC brings the map back.
		const int Block = Clip.Chosen().front();
		S.bEyeSwitched = S.bEyeSwitched || !G.bFilm;
		G.bFilm = true;
		S.Eye.Ride(Block);
		Say("THE FILM RIDES WITH THEM: ESC BRINGS THE MAP BACK");
	}
	else if (Word == TEXT("staff"))
	{
		const std::string Command = Clip.ToStaff();
		if (Command.empty())
		{
			Say("YOUR STAFF ALREADY RUNS IT");
		}
		else
		{
			GiveGeneral(WideOf(Command), TEXT("staff"), -1, TArray<FVector2D>(), 0.0, 0.0);
			G.PingAt = -10.0;
		}
	}
}

// ---- What he cannot see ----------------------------------------------------------------------------------------

bool ALandBattle::HiddenMan(int32 Man) const
{
	if (!State.IsValid() || State->General.FoeHidden.empty() || Man < 0 || static_cast<size_t>(Man) >= State->TrackOfMan.size())
	{
		return false;
	}
	const int Track = State->TrackOfMan[static_cast<size_t>(Man)];
	return Track >= 0 && static_cast<size_t>(Track) < State->Rec.Tracks.size() && State->General.Hides(State->Rec.Tracks[static_cast<size_t>(Track)].Block);
}

bool ALandBattle::HiddenVehicle(int32 Vehicle) const
{
	if (!State.IsValid() || State->General.FoeHidden.empty() || Vehicle < 0 || static_cast<size_t>(Vehicle) >= State->TrackOfVehicle.size())
	{
		return false;
	}
	const int Track = State->TrackOfVehicle[static_cast<size_t>(Vehicle)];
	return Track >= 0 && static_cast<size_t>(Track) < State->Rec.Tracks.size() && State->General.Hides(State->Rec.Tracks[static_cast<size_t>(Track)].Block);
}

// ---- The general's walk pressed in code (-TheaterLandScript=<folder>) -------------------------------------------
// The Mac's builder has no hand on the mouse, so Brian's own steps are pressed through his pointer here, as his hand
// would press them (GeneralPress, GeneralMove, GeneralRelease, GeneralDo): two units carried into the woods before the
// battle, the battle begun, a unit sent forward, one sent at an enemy he can see, one firing into a hedge or a wood, a
// unit's plate held up. Where a place is off the screen the order is given directly, and the log says so. A still of
// each step goes into the folder; then three minutes at the battle's own pace (its frame times in the log every 10 s),
// the rest at four times, the after card's still, and the game quits (or, for a campaign battle opened with FIGHT IT
// YOURSELF, CONTINUE is pressed and the campaign takes the result). LandScript.h makes the choices; the cloud test
// (land_command_test.cpp, command-check.ts) makes the same ones and has the rules take them.

void ALandBattle::ScriptStill(const TCHAR* Name)
{
	if (ScriptFolder.IsEmpty())
	{
		return;
	}
	IFileManager::Get().MakeDirectory(*ScriptFolder, true);
	const FString File = FPaths::Combine(ScriptFolder, Key + TEXT("-general-") + Name + TEXT(".png"));
	FScreenshotRequest::RequestScreenshot(File, true, false);
	++ScriptStills;
	UE_LOG(LogTheater, Display, TEXT("Land battle (script): still %s at %.1f s of battle"), *File, State.IsValid() ? State->Battle : 0.0);
}

void ALandBattle::ScriptNext(int32 Step)
{
	ScriptStep = Step;
	ScriptAt = RealT;
}

void ALandBattle::TickScript()
{
	if (ScriptFolder.IsEmpty() || ScriptStep >= 99 || !State.IsValid())
	{
		return;
	}
	FLandBattleState& S = *State;
	FLandGeneralShown& G = S.General;
	LandCommand::General& Clip = G.Clipboard;
	const double Since = RealT - ScriptAt;
	auto Say = [](const FString& Words)
	{
		UE_LOG(LogTheater, Display, TEXT("Land battle (script): %s"), *Words);
	};
	auto NameOf = [&Clip](int32 Block)
	{
		const LandCommand::Tag* T = Clip.TagOf(Block);
		return T ? FString::Printf(TEXT("%s (block %d)"), *WideOf(T->Name), Block) : FString::Printf(TEXT("block %d"), Block);
	};
	auto Used = [this]()
	{
		std::vector<int> Out;
		for (const int32 Block : ScriptUsed)
		{
			Out.push_back(Block);
		}
		return Out;
	};
	// (a tag on the screen: where his tags hang this frame, else where it would)
	auto TagPixel = [this, &G, &Clip](int32 Block, FVector2D& Out)
	{
		for (const FLandGeneralShown::Mark& M : G.Marks)
		{
			if (M.Block == Block)
			{
				Out = FVector2D(M.X, M.Y);
				return true;
			}
		}
		const LandCommand::Tag* T = Clip.TagOf(Block);
		return T && PixelOf(T->X, T->Y, TagLift, Out);
	};
	// (a field place on the screen, clear of the top bar and his cards, and the ground under it that place)
	auto OnScreen = [this](double X, double Y, FVector2D& Out)
	{
		const FVector2D View = ViewSize();
		double Gx = 0.0, Gy = 0.0;
		return PixelOf(X, Y, 0.0, Out) && Out.X > 24.0 && Out.X < View.X - 24.0 && Out.Y > 90.0 && Out.Y < View.Y * 0.74
			&& GroundUnder(Out, Gx, Gy) && FMath::Sqrt((Gx - X) * (Gx - X) + (Gy - Y) * (Gy - Y)) < 6.0;
	};
	auto GlideTo = [&S](double X, double Y)
	{
		S.GeneralIn.bGoTo = true;
		S.GeneralIn.GoTo = LandPlay::Vec(X, Y, 0.0);
		S.GeneralIn.bFollow = false;
	};
	// (his click on a unit's tag: it alone in hand)
	auto PickUp = [this, &TagPixel](int32 Block)
	{
		FVector2D At(-1.0, -1.0);
		TagPixel(Block, At);
		GeneralPress(At, 0, false, Block);
		GeneralRelease(At, 0, false, Block);
	};
	if (RealT - ScriptAt > 1500.0)
	{
		Say(FString::Printf(TEXT("step %d waited 25 minutes; the walk ends and the game quits"), ScriptStep));
		QuitAt = RealT + 1.0;
		ScriptStep = 99;
		return;
	}

	switch (ScriptStep)
	{
	case 0:
		// His screen up, his tags on it, a few seconds for the first picture to settle.
		if (!G.bOn || Clip.Tags().empty() || G.Marks.empty())
		{
			if (Since > 180.0)
			{
				Say(TEXT("his screen never came up (is it a battle with a side of his own?); the walk ends"));
				ScriptStep = 99;
			}
			return;
		}
		Say(FString::Printf(TEXT("the walk begins: %d of his units, %d tags on the screen, %s; stills into %s"), static_cast<int32>(std::count_if(Clip.Tags().begin(), Clip.Tags().end(), [](const LandCommand::Tag& T) { return T.bMine; })),
			static_cast<int32>(G.Marks.size()), Clip.Deploying() ? TEXT("the setup step open") : TEXT("no setup step"), *ScriptFolder));
		ScriptNext(Clip.Deploying() ? 1 : 20);
		ScriptAt -= 3.0;
		return;

	// ---- The setup step: two of his units carried into the woods ----
	case 1:
	{
		if (Since < 3.0)
		{
			return;
		}
		if (ScriptUsed.Num() == 0)
		{
			ScriptStill(TEXT("01-deploy"));
		}
		std::vector<LandScript::Point> Taken;
		for (const FVector2D& P : ScriptTaken)
		{
			Taken.push_back({ P.X, P.Y });
		}
		int32 Block = LandScript::Mine(Clip, "inf", Used());
		Block = Block >= 0 ? Block : LandScript::Mine(Clip, "", Used());
		double X = 0.0, Y = 0.0;
		if (Block < 0 || !LandScript::WoodsFor(Clip, S.Rec.Ground, Block, Taken, X, Y))
		{
			Say(Block < 0 ? FString(TEXT("no unit of his left to carry")) : TEXT("no woods on his ground for ") + NameOf(Block) + TEXT(": none carried"));
			ScriptNext(5);
			return;
		}
		ScriptUnit = Block;
		ScriptX = X;
		ScriptY = Y;
		const LandCommand::Tag* T = Clip.TagOf(Block);
		GlideTo((T->X + X) * 0.5, (T->Y + Y) * 0.5);
		ScriptNext(2);
		return;
	}
	case 2:
	{
		if (Since < 3.0)
		{
			return;
		}
		FVector2D From, To;
		bScriptByHand = TagPixel(ScriptUnit, From) && OnScreen(ScriptX, ScriptY, To) && (To - From).Size() > 12.0;
		if (bScriptByHand)
		{
			ScriptPixel = To;
			GeneralPress(From, 0, false, ScriptUnit);
			GeneralMove((From + To) * 0.5, -1);
			GeneralMove(To, -1);
			Say(FString::Printf(TEXT("%s carried by his pointer toward the woods at %.0f, %.0f: the pointer says \"%s\""), *NameOf(ScriptUnit), ScriptX, ScriptY, *WideOf(G.Would)));
			if (ScriptUsed.Num() == 0)
			{
				ScriptStill(TEXT("02-carry-into-woods"));
			}
		}
		ScriptNext(3);
		return;
	}
	case 3:
	{
		// (the place followed on the screen, should the view still be settling)
		FVector2D Now;
		if (bScriptByHand && PixelOf(ScriptX, ScriptY, 0.0, Now))
		{
			ScriptPixel = Now;
		}
		if (bScriptByHand && Since < 0.6)
		{
			// (held there, whatever the real pointer does meanwhile)
			GeneralMove(ScriptPixel, -1);
			return;
		}
		const LandCommand::Tag* T = Clip.TagOf(ScriptUnit);
		if (bScriptByHand)
		{
			GeneralRelease(ScriptPixel, 0, false, -1);
			Say(FString::Printf(TEXT("%s set down by his pointer in the woods at %.0f, %.0f"), *NameOf(ScriptUnit), ScriptX, ScriptY));
		}
		else
		{
			PlaceLive(ScriptUnit, ScriptX, ScriptY, T ? T->Face : 0.0);
			Say(FString::Printf(TEXT("%s set down directly in the woods at %.0f, %.0f (the place was off the screen)"), *NameOf(ScriptUnit), ScriptX, ScriptY));
		}
		ScriptUsed.Add(ScriptUnit);
		ScriptTaken.Add(FVector2D(ScriptX, ScriptY));
		ScriptNext(ScriptUsed.Num() < 2 ? 1 : 4);
		return;
	}
	case 4:
		if (Since < 3.0)
		{
			return;
		}
		for (int32 K = 0; K < ScriptUsed.Num(); ++K)
		{
			const LandCommand::Tag* T = Clip.TagOf(ScriptUsed[K]);
			const double Off = T ? FMath::Sqrt((T->X - ScriptTaken[K].X) * (T->X - ScriptTaken[K].X) + (T->Y - ScriptTaken[K].Y) * (T->Y - ScriptTaken[K].Y)) : -1.0;
			Say(FString::Printf(TEXT("%s: %s"), *NameOf(ScriptUsed[K]), Off >= 0.0 && Off < 20.0 ? TEXT("the rules placed it there") : TEXT("NOT where it was set down: the rules refused it (their words are in the line above)")));
		}
		ScriptStill(TEXT("03-in-the-woods"));
		ScriptNext(5);
		return;
	case 5:
		if (Since < 1.0)
		{
			return;
		}
		// (the night patrol's button stands only while men on foot are in his hand: one picked up, as his click on its
		// card would, where none is; the Mac's walk of 23 November 00:39Z had placed them all and put them down)
		if (FLandLive::RuleNow(TEXT("patrol")) && FLandLive::RuleNow(TEXT("hide")) && Clip.Waiters().empty())
		{
			const int32 OnFoot = LandScript::Mine(Clip, "inf", {});
			if (OnFoot >= 0)
			{
				Clip.Pick(OnFoot, false);
				Say(FString::Printf(TEXT("%s taken in hand for the night patrol"), *NameOf(OnFoot)));
			}
		}
		ScriptNext(8);
		return;
	case 8:
		if (Since < 1.0)
		{
			return;
		}
		// (the night patrol's button, pressed as his click would press it, while he places his army: the Mac's
		// click-through of the Battle rules card's night patrol, 22 November)
		if (!FLandLive::RuleNow(TEXT("patrol")) || !FLandLive::RuleNow(TEXT("hide")))
		{
			Say(Screen.IsValid() && Screen->PressDrawn(TEXT("general:patrol"))
				? FString(TEXT("SEND A PATROL TONIGHT was on his screen with night patrol switched off: A FAULT"))
				: FString(TEXT("night patrol (or forest ambushes) switched off on the Battle rules card: no SEND A PATROL TONIGHT button, as it should be")));
		}
		else if (Screen.IsValid() && Screen->PressDrawn(TEXT("general:patrol")))
		{
			Say(TEXT("SEND A PATROL TONIGHT pressed (his click on the button)"));
		}
		else
		{
			Say(Clip.Waiters().empty() ? FString(TEXT("no SEND A PATROL TONIGHT button: no men on foot left in his hand"))
				: FString(TEXT("SEND A PATROL TONIGHT NOT on his screen though night patrol is on and men on foot are in hand: A FAULT")));
		}
		ScriptNext(7);
		return;
	case 7:
		if (Since < 2.0)
		{
			return;
		}
		ScriptStill(TEXT("03b-patrol"));
		GeneralDo(TEXT("begin"));
		Say(TEXT("the battle begun (his BEGIN)"));
		ScriptNext(6);
		return;
	case 6:
		if (Clip.Deploying())
		{
			if (Since > 30.0)
			{
				Say(TEXT("the setup step still open 30 s after BEGIN; the walk goes on"));
				ScriptNext(20);
			}
			return;
		}
		if (Since < 5.0)
		{
			return;
		}
		for (const int32 Block : ScriptUsed)
		{
			const LandCommand::Tag* T = Clip.TagOf(Block);
			Say(FString::Printf(TEXT("%s: %s"), *NameOf(Block), T && T->bHidden ? TEXT("hidden from the enemy, the forest mark on its tag") : TEXT("NOT hidden")));
		}
		ScriptStill(TEXT("04-battle-begun"));
		ScriptNext(20);
		return;

	// ---- The battle: a unit sent forward ----
	case 20:
	{
		if (S.Battle < 6.0 || Clip.Deploying())
		{
			return;
		}
		int32 Block = LandScript::Mine(Clip, "inf", Used());
		Block = Block >= 0 ? Block : LandScript::Mine(Clip, "", Used());
		double X = 0.0, Y = 0.0;
		if (Block < 0 || !LandScript::Forward(Clip, S.Rec.Ground, Block, 120.0, X, Y))
		{
			Say(TEXT("no unit of his to send forward; on to the attack"));
			ScriptNext(30);
			return;
		}
		ScriptUnit = Block;
		ScriptX = X;
		ScriptY = Y;
		const LandCommand::Tag* T = Clip.TagOf(Block);
		GlideTo((T->X + X) * 0.5, (T->Y + Y) * 0.5);
		ScriptNext(21);
		return;
	}
	case 21:
	{
		if (Since < 3.0)
		{
			return;
		}
		PickUp(ScriptUnit);
		FVector2D To;
		ScriptGiven = S.Battle;
		if (OnScreen(ScriptX, ScriptY, To))
		{
			GeneralMove(To, -1);
			const FString Would = WideOf(G.Would);
			GeneralPress(To, 0, false, -1);
			GeneralRelease(To, 0, false, -1);
			Say(FString::Printf(TEXT("%s picked up and sent by his click to %.0f, %.0f (the pointer said \"%s\")"), *NameOf(ScriptUnit), ScriptX, ScriptY, *Would));
		}
		else
		{
			GiveGeneral(WideOf(Clip.Move(ScriptX, ScriptY, false)), TEXT("move"), -1, TArray<FVector2D>({ FVector2D(ScriptX, ScriptY) }), ScriptX, ScriptY);
			Say(FString::Printf(TEXT("%s sent directly to %.0f, %.0f (the place was off the screen)"), *NameOf(ScriptUnit), ScriptX, ScriptY));
		}
		ScriptUsed.Add(ScriptUnit);
		ScriptNext(22);
		return;
	}
	case 22:
		if (Since < 1.5)
		{
			return;
		}
		ScriptStill(TEXT("05-move-order"));
		ScriptNext(23);
		return;
	case 23:
	{
		const LandCommand::Tag* T = Clip.TagOf(ScriptUnit);
		if (T && T->OrderIn < 0.0 && T->OrderKind == "move")
		{
			Say(FString::Printf(TEXT("%s: the order reached it %.0f s of battle after he gave it (the radio's delay)"), *NameOf(ScriptUnit), S.Battle - ScriptGiven));
			ScriptNext(30);
		}
		else if (Since > 40.0)
		{
			Say(FString::Printf(TEXT("%s: the move not seen to arrive in 40 s (its order \"%s\", on the air %.0f s)"), *NameOf(ScriptUnit), T ? *WideOf(T->OrderKind) : TEXT("?"), T ? T->OrderIn : -1.0));
			ScriptNext(30);
		}
		return;
	}

	// ---- One sent at an enemy he can see ----
	case 30:
	{
		int32 Block = LandScript::Mine(Clip, "arm", Used());
		Block = Block >= 0 ? Block : LandScript::Mine(Clip, "inf", Used());
		Block = Block >= 0 ? Block : LandScript::Mine(Clip, "", Used());
		const LandCommand::Tag* T = Block >= 0 ? Clip.TagOf(Block) : nullptr;
		const int32 Foe = T ? LandScript::Foe(Clip, T->X, T->Y) : -1;
		if (Foe < 0)
		{
			if (Since > 120.0 || !T)
			{
				Say(T ? FString(TEXT("no enemy seen in two minutes: no attack")) : FString(TEXT("no unit of his left to attack with")));
				ScriptNext(40);
			}
			return;
		}
		ScriptUnit = Block;
		ScriptFoe = Foe;
		const LandCommand::Tag* F = Clip.TagOf(Foe);
		GlideTo((T->X + F->X) * 0.5, (T->Y + F->Y) * 0.5);
		ScriptNext(31);
		return;
	}
	case 31:
	{
		if (Since < 3.0)
		{
			return;
		}
		PickUp(ScriptUnit);
		FVector2D At(-1.0, -1.0);
		TagPixel(ScriptFoe, At);
		GeneralMove(At, ScriptFoe);
		const FString Would = WideOf(G.Would);
		GeneralPress(At, 0, false, ScriptFoe);
		GeneralRelease(At, 0, false, ScriptFoe);
		Say(FString::Printf(TEXT("%s picked up and sent by his click at %s (the pointer said \"%s\")"), *NameOf(ScriptUnit), *NameOf(ScriptFoe), *Would));
		ScriptUsed.Add(ScriptUnit);
		ScriptNext(32);
		return;
	}
	case 32:
		if (Since < 1.5)
		{
			return;
		}
		ScriptStill(TEXT("06-attack"));
		ScriptNext(40);
		return;

	// ---- One firing into cover: FIRE ON, then a click on a hedge or a wood ----
	case 40:
	{
		int32 Block = LandScript::Mine(Clip, "inf", Used());
		Block = Block >= 0 ? Block : LandScript::Mine(Clip, "", Used());
		Block = Block >= 0 ? Block : (ScriptUsed.Num() > 0 ? ScriptUsed.Last() : -1);
		std::vector<std::vector<LandScript::Point>> Hedges;
		if (S.Field)
		{
			for (const LandField::HedgeLine& H : S.Field->Hedges())
			{
				// (the kinds handed to the rules: a hedgerow, a bank, a garden's hedge, a wall)
				if (H.Kind == 0 || H.Kind == 1 || H.Kind == 3 || H.Kind == 4)
				{
					Hedges.push_back(H.Points);
				}
			}
		}
		double X = 0.0, Y = 0.0;
		const LandScript::Cover Kind = Block >= 0 ? LandScript::CoverFor(Clip, S.Rec.Ground, Hedges, Block, X, Y) : LandScript::Cover::None;
		if (Kind == LandScript::Cover::None && (Block < 0 || !LandScript::Forward(Clip, S.Rec.Ground, Block, 150.0, X, Y)))
		{
			Say(TEXT("no unit and no cover to fire into; on to the plate"));
			ScriptNext(50);
			return;
		}
		Say(FString::Printf(TEXT("%s will fire into %s at %.0f, %.0f"), *NameOf(Block), Kind == LandScript::Cover::Hedge ? TEXT("a hedge") : Kind == LandScript::Cover::Woods ? TEXT("a wood") : TEXT("open ground (no cover ahead of it)"), X, Y));
		ScriptUnit = Block;
		ScriptX = X;
		ScriptY = Y;
		const LandCommand::Tag* T = Clip.TagOf(Block);
		GlideTo((T->X + X) * 0.5, (T->Y + Y) * 0.5);
		ScriptNext(41);
		return;
	}
	case 41:
	{
		if (Since < 3.0)
		{
			return;
		}
		PickUp(ScriptUnit);
		GeneralDo(TEXT("fire"));
		FVector2D To;
		bScriptByHand = OnScreen(ScriptX, ScriptY, To);
		ScriptGiven = S.Battle;
		if (bScriptByHand)
		{
			ScriptPixel = To;
			GeneralMove(To, -1);
			Say(FString::Printf(TEXT("FIRE ON armed for %s; over the cover the pointer says \"%s\""), *NameOf(ScriptUnit), *WideOf(G.Would)));
			ScriptStill(TEXT("07-fire-on-cover"));
		}
		ScriptNext(42);
		return;
	}
	case 42:
	{
		FVector2D Now;
		if (bScriptByHand && PixelOf(ScriptX, ScriptY, 0.0, Now))
		{
			ScriptPixel = Now;
		}
		if (bScriptByHand && Since < 0.6)
		{
			GeneralMove(ScriptPixel, -1);
			return;
		}
		if (bScriptByHand)
		{
			GeneralPress(ScriptPixel, 0, false, -1);
			GeneralRelease(ScriptPixel, 0, false, -1);
			Say(FString::Printf(TEXT("%s fires into the cover at %.0f, %.0f by his click"), *NameOf(ScriptUnit), ScriptX, ScriptY));
		}
		else
		{
			GiveGeneral(WideOf(Clip.FireInto(ScriptX, ScriptY)), TEXT("fire"), -1, TArray<FVector2D>(), ScriptX, ScriptY);
			G.Armed.clear();
			Say(FString::Printf(TEXT("%s fires into the cover at %.0f, %.0f, given directly (the place was off the screen)"), *NameOf(ScriptUnit), ScriptX, ScriptY));
		}
		if (!ScriptUsed.Contains(ScriptUnit))
		{
			ScriptUsed.Add(ScriptUnit);
		}
		ScriptNext(43);
		return;
	}
	case 43:
		if (Since < 2.0)
		{
			return;
		}
		ScriptStill(TEXT("08-fire-into-cover"));
		ScriptNext(50);
		return;

	// ---- A unit's plate held up, as his pointer would hover over its tag ----
	case 50:
	{
		int32 Block = -1;
		for (const int32 Each : ScriptUsed)
		{
			const LandCommand::Tag* T = Clip.TagOf(Each);
			if (Block < 0 && T && T->Ratio > 0 && LandScript::Standing(*T))
			{
				Block = Each;
			}
		}
		Block = Block >= 0 ? Block : LandScript::Mine(Clip, "", {});
		if (Block < 0)
		{
			ScriptNext(52);
			return;
		}
		ScriptUnit = Block;
		G.ShowPlate = Block;
		S.GeneralIn.GoBlock = Block;
		S.GeneralIn.bFollow = false;
		ScriptNext(51);
		return;
	}
	case 51:
	{
		if (Since < 3.0)
		{
			return;
		}
		const LandCommand::Tag* T = Clip.TagOf(ScriptUnit);
		if (T)
		{
			Say(FString::Printf(TEXT("the plate of %s: \"%s\", of the %s, about %d men, drawn 1:%d"), *NameOf(ScriptUnit), *WideOf(T->Name).ToUpper(), *WideOf(T->Parent), T->Men >= 0 ? T->Men : T->Men0, T->Ratio));
		}
		ScriptStill(TEXT("09-plate"));
		ScriptNext(52);
		return;
	}
	case 52:
		if (Since < 0.5)
		{
			return;
		}
		G.ShowPlate = -1;
		GeneralDo(TEXT("clear"));
		Say(TEXT("the walk's steps done; three minutes now at the battle's own pace (the frame times are the \"Land battle:\" lines every 10 s)"));
		ScriptNext(60);
		return;

	// ---- Three minutes at its own pace, the rest at four times, the after card ----
	case 60:
		if (Since < 30.0)
		{
			return;
		}
		ScriptStill(TEXT("10-mid-battle"));
		ScriptNext(61);
		ScriptAt -= 30.0;
		return;
	case 61:
		if (Since < 180.0 && !S.bCard)
		{
			return;
		}
		Say(TEXT("three minutes held at the battle's own pace; the rest at four times, to the after card"));
		SetSpeed(4.0);
		ScriptNext(70);
		return;
	case 70:
		if (!S.bCard)
		{
			return;
		}
		ScriptNext(71);
		return;
	case 71:
		if (Since < 1.5)
		{
			return;
		}
		ScriptStill(TEXT("11-after-card"));
		if (HandBack)
		{
			// (a campaign battle he opened with FIGHT IT YOURSELF: CONTINUE pressed as his hand would, and the campaign's
			// attack card takes the result; the game goes on, for the map's own steps after it)
			Say(FString::Printf(TEXT("the walk done: %d stills; CONTINUE, back to the campaign"), ScriptStills));
			ScriptStep = 99;
			Press(TEXT("continue"));
			return;
		}
		Say(FString::Printf(TEXT("the walk done: %d stills; the game quits"), ScriptStills));
		QuitAt = RealT + 3.0;
		ScriptStep = 99;
		return;
	default:
		return;
	}
}
