#include "Battle/AirStage.h"

#include "Battle/AirFeed.h"
#include "Battle/AirGround.h"
#include "Battle/SeaFace.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Campaign/ArmyPiece.h"
#include "Campaign/CampaignEnvironment.h"
#include "Campaign/CampaignMapActor.h"
#include "Campaign/CampaignMiniatures.h"
#include "Campaign/CampaignTextures.h"
#include "Campaign/TheaterStillMesh.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "ProceduralMeshComponent.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "TheaterCommand.h"
#include "UnrealClient.h"

namespace StageParts
{
	static double Ease(double X)
	{
		X = FMath::Clamp(X, 0.0, 1.0);
		return X * X * (3.0 - 2.0 * X);
	}

	static double EaseIn(double X)
	{
		X = FMath::Clamp(X, 0.0, 1.0);
		return X * X;
	}

	/** The model a campaign class is drawn with (the miniature kit's names: "Battleship:USA" and so on). */
	static const TCHAR* KindOf(const FString& Class)
	{
		return Class == TEXT("bb") || Class == TEXT("ag") ? TEXT("Battleship") : Class == TEXT("cv") ? TEXT("Carrier") : Class == TEXT("dd") ? TEXT("Destroyer")
			: Class == TEXT("ss") ? TEXT("Submarine") : TEXT("Cruiser");
	}

	/**
	 * An American ship in the stage's paint, Measure 1 of 1941: the miniature kit's navy grey (the unit table's 5F6E80,
	 * a light blue-grey) to Dark Gray 5-D, and her wooden decks (the kit's B79E74) stained Deck Blue 20-B, as the
	 * battleships at Pearl Harbor wore them. From above, her planks were most of what showed, and Battleship Row read
	 * tan (the art director on 2b177386's Pearl 06, 22 November 04:16Z: "Measure 1 is a cool dark grey"). Each colour
	 * within a few levels of the kit's is moved by the same step, so any shading the kit laid on it stays; the map's
	 * own miniatures keep the kit's paint. Returns how many points were repainted.
	 */
	static int32 InStagePaint(FMiniatureMesh& Model, const FString& Nation)
	{
		if (Nation != TEXT("USA"))
		{
			return 0;
		}
		struct FRepaint
		{
			FColor From, To;
		};
		static const FRepaint Repaints[] = {
			{ FColor(0x5F, 0x6E, 0x80), FColor(0x4E, 0x54, 0x5A) }, // (navy grey to Dark Gray 5-D)
			{ FColor(0xB7, 0x9E, 0x74), FColor(0x3E, 0x47, 0x55) }, // (teak planks to Deck Blue 20-B)
		};
		int32 Count = 0;
		for (FColor& C : Model.Colors)
		{
			for (const FRepaint& R : Repaints)
			{
				const int32 DR = static_cast<int32>(C.R) - R.From.R, DG = static_cast<int32>(C.G) - R.From.G, DB = static_cast<int32>(C.B) - R.From.B;
				if (FMath::Abs(DR) <= 14 && FMath::Abs(DG) <= 14 && FMath::Abs(DB) <= 14)
				{
					C.R = static_cast<uint8>(FMath::Clamp(R.To.R + DR, 0, 255));
					C.G = static_cast<uint8>(FMath::Clamp(R.To.G + DG, 0, 255));
					C.B = static_cast<uint8>(FMath::Clamp(R.To.B + DB, 0, 255));
					++Count;
					break;
				}
			}
		}
		return Count;
	}

	/**
	 * The kit's model of her class stretched to her own size (AirFeed::HullOf): along her length, across her beam, and
	 * up by the beam's share, so that her upperworks keep their look; the normals follow. The kit has one model a class
	 * and navy (the American battleship an Iowa, 270 m), and Battleship Row's 1941 berths are laid for ships of 178 to
	 * 190 m: Brian on the film of build 1768 saw them on top of each other.
	 */
	static void FitHull(FMiniatureMesh& Model, const FStageShip& Given, double U)
	{
		if (Model.Vertices.Num() == 0)
		{
			return;
		}
		FBox Bounds(ForceInit);
		for (const FVector& Point : Model.Vertices)
		{
			Bounds += Point;
		}
		const FVector Size = Bounds.GetSize() / U;
		const AirFeed::HullSize Want = AirFeed::HullOf(std::string(TCHAR_TO_UTF8(*Given.Class)), std::string(TCHAR_TO_UTF8(*Given.Name)), std::string(TCHAR_TO_UTF8(*Given.Nation)));
		if (Size.X < 1.0 || Size.Y < 1.0 || Want.Long <= 0.0 || Want.Beam <= 0.0)
		{
			return;
		}
		const FVector Stretch(Want.Long / Size.X, Want.Beam / Size.Y, Want.Beam / Size.Y);
		for (FVector& Point : Model.Vertices)
		{
			Point *= Stretch;
		}
		for (FVector& Normal : Model.Normals)
		{
			Normal = (Normal / Stretch).GetSafeNormal();
		}
	}

	/**
	 * (The effects thread's oil fires at sea, passed by the art director 19 November 04:48Z, kit 5cd0115.) Oil burning
	 * where a ship went down, or on a burning ship's deck: one column for each ship lost, until the scene ends. Written
	 * so that it builds before the kit's OilFire reaches unreal and after: the call is made only where the kit has it.
	 * SinceSeconds: how long the fire has burned already, so the kit stands its column grown (the effects kit's 01f2dfc7).
	 */
	template <typename TKit>
	static bool OilFire(TKit* Kit, int32 Id, const FVector& WorldAt, float ReachMetres, const TCHAR* Kind, float SinceSeconds = 0.0f)
	{
		if constexpr (requires(TKit* K, int32 I, const FVector& At, float R, const FString& W, double Until, float Since) { K->OilFire(I, At, R, W, Until, Since); })
		{
			if (Kit)
			{
				Kit->OilFire(Id, WorldAt, ReachMetres, FString(Kind), 0.0, SinceSeconds);
				return true;
			}
		}
		return false;
	}

	template <typename TKit>
	static void OilStop(TKit* Kit, int32 Id)
	{
		if constexpr (requires(TKit* K, int32 I) { K->OilStop(I); })
		{
			if (Kit)
			{
				Kit->OilStop(Id);
			}
		}
	}

	/** How much a ship's oil column matters to the picture: the capital ships first, then the cruisers, then the rest. */
	static int32 OilRank(const FString& Class)
	{
		return Class == TEXT("bb") || Class == TEXT("cv") || Class == TEXT("ag") ? 3 : Class == TEXT("dd") ? 1 : 2;
	}

	static TArray<double> Numbers(const TSharedPtr<FJsonValue>& Value)
	{
		TArray<double> Out;
		const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
		if (Value.IsValid() && Value->TryGetArray(List))
		{
			Out.Reserve(List->Num());
			for (const TSharedPtr<FJsonValue>& Item : *List)
			{
				Out.Add(Item.IsValid() ? Item->AsNumber() : 0.0);
			}
		}
		return Out;
	}

	static const FLinearColor TankPaint = FLinearColor(FColor::FromHex(TEXT("BDB6A2")));
	static const FLinearColor HangarPaint = FLinearColor(FColor::FromHex(TEXT("8C8A80")));
	static const FLinearColor RoofPaint = FLinearColor(FColor::FromHex(TEXT("6F6E68")));
	// The yard's (the historian's 1941 blocks): the pads, the apron and the docks' coping a weathered concrete, a
	// drained dock's floor dark, the floating dock's steel the grey of the ships beside it.
	// (about 140 luma: the art director, 17:45Z; a cool grey, 22 November: the warm 8F8B83 read tan under the morning
	// sun, "the quays should be grey, not tan", the art director on 2b177386's frames, 04:16Z; Oahu's stage paints its
	// quays the same)
	static const FLinearColor ConcretePaint = FLinearColor(FColor::FromHex(TEXT("8A8D90")));
	static const FLinearColor DockFloorPaint = FLinearColor(FColor::FromHex(TEXT("4A4844")));
	static const FLinearColor DockSteelPaint = FLinearColor(FColor::FromHex(TEXT("5C6062")));
	static constexpr double YardSkirt = -4.5; // metres under a yard block's ground its sides reach down to
	static const FLinearColor TrunkPaint = FLinearColor(FColor::FromHex(TEXT("7A6A55")));
	static const FLinearColor FrondPaint = FLinearColor(FColor::FromHex(TEXT("3F6A2E")));
	// A deep blue, 1.7 times the light of the 1C4453 it was and bluer (the art director on build 1778's Oahu 06: the
	// harbour read 34 of brightness and grey-teal looking down on the ships, the brief's near sea is 50 to 70; the
	// battle's grade takes nearly half the colour out of the darks, so the paint carries more blue than the picture
	// shows). The first run's 24505F measured 100 to 118 looking down from 5 km, against her mark of 80 or under.
	static const FColor SeaPaint = FColor::FromHex(TEXT("1F577A"));
	/**
	 * The sea's paint under the day's sun: as it is up to a sun 20 degrees high, and darker as the sun climbs, to 0.55
	 * of it at 45 degrees and over, its hue kept (the art director on build 1732: Midway's sun at 45 lit the open sea
	 * to 78 to 90 of brightness and teal, against Pearl Harbor's 42 to 47 under its morning sun; her mark there was 55
	 * to 60). At 0.55 Midway's sea keeps the light it had under the old paint at 0.65, and a little more (the material
	 * takes the paint to the power 2.2: 0.55 of the new paint is 1.17 times the light of 0.65 of the old).
	 */
	static FColor SeaPaintUnder(double SunElevationDegrees)
	{
		const double Darker = 1.0 - 0.45 * FMath::Clamp((SunElevationDegrees - 20.0) / 25.0, 0.0, 1.0);
		auto Byte = [Darker](uint8 Channel) { return static_cast<uint8>(FMath::Clamp(FMath::RoundToInt32(Channel * Darker), 0, 255)); };
		return FColor(Byte(SeaPaint.R), Byte(SeaPaint.G), Byte(SeaPaint.B));
	}
}

AAirStage::AAirStage()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false; // (only the stage test ticks itself: a battle's scene calls Advance)
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
}

void AAirStage::EndPlay(const EEndPlayReason::Type Reason)
{
	Looks.End();
	if (OwnEffects.IsValid())
	{
		OwnEffects->End();
		OwnEffects.Reset();
	}
	Effects = nullptr;
	Super::EndPlay(Reason);
}

// ---------------------------------------------------------------------------------------------------------------
// Where things are: stage metres, the board, this actor's frame, the world.

FVector2D AAirStage::StageToBoard(const FVector2D& Stage) const
{
	const double DX = Stage.X - StageAim.X, DY = Stage.Y - StageAim.Y, C = FMath::Cos(StageTurn), S = FMath::Sin(StageTurn);
	return FVector2D(BoardTarget.X + DX * C - DY * S, BoardTarget.Y + DX * S + DY * C);
}

FVector2D AAirStage::BoardToStage(const FVector2D& Board) const
{
	const double DX = Board.X - BoardTarget.X, DY = Board.Y - BoardTarget.Y, C = FMath::Cos(StageTurn), S = FMath::Sin(StageTurn);
	return FVector2D(StageAim.X + DX * C + DY * S, StageAim.Y - DX * S + DY * C);
}

FVector AAirStage::BoardToLocal(const FVector2D& Board, double ZMetres) const
{
	// (up the board is the actor's X, across it its Y: the world's way, X ahead and Y to the right, seen from above)
	return FVector(Board.Y, Board.X, ZMetres) * UnitsPerMetre;
}

FVector AAirStage::StageToLocal(const FVector& Stage) const
{
	return BoardToLocal(StageToBoard(FVector2D(Stage.X, Stage.Y)), Stage.Z);
}

FVector AAirStage::StageToWorld(const FVector& Stage) const
{
	return GetActorTransform().TransformPosition(StageToLocal(Stage));
}

double AAirStage::HeadingToYaw(double BoardHeading) const
{
	// A heading on the board (0 up it, clockwise) is the yaw of the same way in the actor's frame.
	return FMath::RadiansToDegrees(BoardHeading);
}

double AAirStage::StageHeight(double East, double North) const
{
	if (GridCols <= 0 || GridRows <= 0 || GridCell <= 0.0)
	{
		return 0.0;
	}
	const int32 C = FMath::FloorToInt((East - GridE0) / GridCell), R = FMath::FloorToInt((North - GridN0) / GridCell);
	if (C < 0 || R < 0 || C >= GridCols || R >= GridRows)
	{
		return 0.0;
	}
	return FMath::Max(0.0, static_cast<double>(GridZ[R * GridCols + C]));
}

double AAirStage::DockFloor(const FVector2D& Stage) const
{
	// A dry dock's basin (the yard's kind 1: drawn drained, its floor 0.2 m over the ground) or a floating dock's
	// pontoon (kind 3: 1.5 m up), as tools/stages/yard.py lays them; the cloud test finds the docked ships inside.
	for (const TArray<double>& Block : Yard)
	{
		const int32 Kind = Block.Num() >= 8 ? static_cast<int32>(Block[7]) : -1;
		if (Kind != 1 && Kind != 3)
		{
			continue;
		}
		const double Compass = FMath::DegreesToRadians(Block[6]);
		const double Dx = Stage.X - Block[0], Dy = Stage.Y - Block[1];
		const double Ahead = Dx * FMath::Sin(Compass) + Dy * FMath::Cos(Compass), Aside = Dx * FMath::Cos(Compass) - Dy * FMath::Sin(Compass);
		const double BasinLong = Kind == 1 ? Block[3] - 20.0 : Block[3], BasinWide = Kind == 1 ? Block[4] - 15.0 : Block[4] - 6.0;
		if (FMath::Abs(Ahead) <= BasinLong * 0.5 && FMath::Abs(Aside) <= BasinWide * 0.5)
		{
			return Kind == 1 ? Block[2] + 0.2 : 1.5;
		}
	}
	return 0.0;
}

double AAirStage::GroundZ(const FVector& Where) const
{
	const FVector Local = GetActorTransform().InverseTransformPosition(Where);
	const FVector2D Stage = BoardToStage(FVector2D(Local.Y, Local.X) / UnitsPerMetre);
	return GetActorTransform().TransformPosition(FVector(Local.X, Local.Y, StageHeight(Stage.X, Stage.Y) * UnitsPerMetre)).Z;
}

std::function<double(double, double)> AAirStage::GroundAside() const
{
	struct FKept
	{
		FTransform Actor;
		FVector2D Aim, Target;
		double Turn = 0.0, U = 100.0, E0 = 0.0, N0 = 0.0, Cell = 0.0;
		int32 Cols = 0, Rows = 0;
		TArray<int16> Z;
	};
	TSharedRef<FKept, ESPMode::ThreadSafe> Kept = MakeShared<FKept, ESPMode::ThreadSafe>();
	Kept->Actor = GetActorTransform();
	Kept->Aim = StageAim;
	Kept->Target = BoardTarget;
	Kept->Turn = StageTurn;
	Kept->U = UnitsPerMetre;
	Kept->E0 = GridE0;
	Kept->N0 = GridN0;
	Kept->Cell = GridCell;
	if (bRaised && GridCols > 0 && GridRows > 0 && GridZ.Num() == GridCols * GridRows)
	{
		Kept->Cols = GridCols;
		Kept->Rows = GridRows;
		Kept->Z = GridZ;
	}
	// (GroundZ's own steps, over the copy: the board point as the scene puts it in the world, into this actor's frame,
	// onto the stage, the grid's height there, and back up to the world)
	return [Kept](double X, double Y)
	{
		const FKept& K = *Kept;
		if (K.Cols <= 0 || K.Rows <= 0 || K.Cell <= 0.0)
		{
			return 0.0;
		}
		const FVector Local = K.Actor.InverseTransformPosition(FVector(Y * K.U, X * K.U, 0.0));
		const double DX = Local.Y / K.U - K.Target.X, DY = Local.X / K.U - K.Target.Y, C = FMath::Cos(K.Turn), S = FMath::Sin(K.Turn);
		const double East = K.Aim.X + DX * C + DY * S, North = K.Aim.Y - DX * S + DY * C;
		const int32 Col = FMath::FloorToInt((East - K.E0) / K.Cell), Row = FMath::FloorToInt((North - K.N0) / K.Cell);
		const double Height = Col < 0 || Row < 0 || Col >= K.Cols || Row >= K.Rows ? 0.0 : FMath::Max(0.0, static_cast<double>(K.Z[Row * K.Cols + Col]));
		return FMath::Max(0.0, K.Actor.TransformPosition(FVector(Local.X, Local.Y, Height * K.U)).Z / K.U);
	};
}

bool AAirStage::ShipAt(int32 Id, FVector& Out) const
{
	const FShip* Ship = Ships.Find(Id);
	if (!Ship)
	{
		return false;
	}
	Out = GetActorTransform().TransformPosition(Ship->Hull ? Ship->Hull->GetRelativeLocation() : Ship->Base);
	return true;
}

bool AAirStage::ShipLie(int32 Id, double& OutLong, FVector& OutBow, bool& bOutHarbour) const
{
	const FShip* Ship = Ships.Find(Id);
	if (!Ship)
	{
		return false;
	}
	OutLong = Ship->Long;
	bOutHarbour = Ship->bHarbour;
	// (a model is built with its bow toward -X)
	FVector Bow = GetActorRotation().RotateVector(FRotator(0.0, Ship->Yaw, 0.0).RotateVector(-FVector::ForwardVector));
	Bow.Z = 0.0;
	OutBow = Bow.GetSafeNormal();
	return !OutBow.IsNearlyZero();
}

FVector AAirStage::TowardSun() const
{
	const double Heading = FMath::DegreesToRadians(SunAzimuth) - StageTurn, Up = FMath::DegreesToRadians(SunElevation);
	const FVector Local(FMath::Cos(Up) * FMath::Cos(Heading), FMath::Cos(Up) * FMath::Sin(Heading), FMath::Sin(Up));
	return GetActorRotation().RotateVector(Local).GetSafeNormal();
}

void AAirStage::SmokeFeet(int32 Id, double Near, TArray<FVector>& Out) const
{
	const FShip* Her = Ships.Find(Id);
	const FVector HerAt = Her ? Her->Base : FVector::ZeroVector;
	for (const TPair<int32, FShip>& Pair : Ships)
	{
		const FShip& Ship = Pair.Value;
		const bool bNeighbour = Her && FVector::Dist(Ship.Base, HerAt) <= Near * UnitsPerMetre;
		if (Ship.Hits > 0 || Ship.SunkAt >= 0.0 || bNeighbour)
		{
			Out.Add(GetActorTransform().TransformPosition(Ship.Base));
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------
// The stage's file.

bool AAirStage::ReadStage(const FString& Key)
{
	const FString Path = FPaths::ProjectContentDir() / TEXT("Data") / TEXT("Stages") / (Key + TEXT(".json"));
	FString Text;
	TSharedPtr<FJsonObject> File;
	if (!FFileHelper::LoadFileToString(Text, *Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), File) || !File.IsValid())
	{
		UE_LOG(LogTheater, Warning, TEXT("Stage: %s is missing or unreadable (tools/stages/mesh.py makes it)."), *Path);
		return false;
	}
	Palette.Reset();
	PaletteNames.Reset();
	const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
	if (File->TryGetArrayField(TEXT("palette"), List))
	{
		for (const TSharedPtr<FJsonValue>& Item : *List)
		{
			const TSharedPtr<FJsonObject>* Paint = nullptr;
			FString Hex, PaintName;
			const bool bPaint = Item->TryGetObject(Paint);
			Palette.Add(bPaint && (*Paint)->TryGetStringField(TEXT("hex"), Hex) ? FColor::FromHex(Hex) : FColor(128, 128, 128));
			if (bPaint)
			{
				(*Paint)->TryGetStringField(TEXT("name"), PaintName);
			}
			PaletteNames.Add(PaintName);
		}
	}
	Parts.Reset();
	if (File->TryGetArrayField(TEXT("meshes"), List))
	{
		for (const TSharedPtr<FJsonValue>& Item : *List)
		{
			const TSharedPtr<FJsonObject>* Mesh = nullptr;
			if (!Item->TryGetObject(Mesh))
			{
				continue;
			}
			FMeshPart& Part = Parts.AddDefaulted_GetRef();
			(*Mesh)->TryGetStringField(TEXT("name"), Part.Name);
			const TArray<double> V = StageParts::Numbers((*Mesh)->TryGetField(TEXT("v")));
			const TArray<double> C = StageParts::Numbers((*Mesh)->TryGetField(TEXT("c")));
			const TArray<double> T = StageParts::Numbers((*Mesh)->TryGetField(TEXT("t")));
			if (V.Num() != C.Num() * 3 || T.Num() % 3 != 0 || (T.Num() > 0 && C.Num() == 0)) // (triangles with no points: the pre-check on round 6)
			{
				UE_LOG(LogTheater, Warning, TEXT("Stage: the %s mesh of %s does not add up; left out."), *Part.Name, *Key);
				Parts.Pop();
				continue;
			}
			// (decimetres in the file, to keep it small)
			Part.Points.Reserve(C.Num());
			for (int32 K = 0; K + 2 < V.Num(); K += 3)
			{
				Part.Points.Add(FVector(V[K], V[K + 1], V[K + 2]) * 0.1);
			}
			for (const double Paint : C)
			{
				Part.Paints.Add(FMath::Clamp(static_cast<int32>(Paint), 0, FMath::Max(Palette.Num() - 1, 0)));
			}
			for (const double Index : T)
			{
				const int32 I = static_cast<int32>(Index);
				Part.Triangles.Add(Part.Points.IsValidIndex(I) ? I : 0);
			}
		}
	}
	auto Rows = [&File](const TCHAR* Field, TArray<TArray<double>>& Out)
	{
		Out.Reset();
		const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
		if (File->TryGetArrayField(Field, Items))
		{
			for (const TSharedPtr<FJsonValue>& Item : *Items)
			{
				Out.Add(StageParts::Numbers(Item));
			}
		}
	};
	Rows(TEXT("tanks"), Tanks);
	Rows(TEXT("hangars"), Hangars);
	Rows(TEXT("palms"), Palms);
	Rows(TEXT("yard"), Yard);
	const TSharedPtr<FJsonObject>* Sun = nullptr;
	if (File->TryGetObjectField(TEXT("sun"), Sun))
	{
		(*Sun)->TryGetNumberField(TEXT("azimuth"), SunAzimuth);
		(*Sun)->TryGetNumberField(TEXT("elevation"), SunElevation);
	}
	const TSharedPtr<FJsonObject>* Ground = nullptr;
	GridCols = GridRows = 0;
	GridZ.Reset();
	if (File->TryGetObjectField(TEXT("ground"), Ground))
	{
		(*Ground)->TryGetNumberField(TEXT("e0"), GridE0);
		(*Ground)->TryGetNumberField(TEXT("n0"), GridN0);
		(*Ground)->TryGetNumberField(TEXT("cell"), GridCell);
		(*Ground)->TryGetNumberField(TEXT("cols"), GridCols);
		(*Ground)->TryGetNumberField(TEXT("rows"), GridRows);
		const TArray<double> Z = StageParts::Numbers((*Ground)->TryGetField(TEXT("z")));
		if (Z.Num() == GridCols * GridRows)
		{
			GridZ.Reserve(Z.Num());
			for (const double H : Z)
			{
				GridZ.Add(static_cast<int16>(FMath::Clamp(H, -32000.0, 32000.0)));
			}
		}
		else
		{
			GridCols = GridRows = 0;
		}
	}
	const TSharedPtr<FJsonObject>* Test = nullptr;
	TestBook.Reset();
	if (File->TryGetObjectField(TEXT("test"), Test))
	{
		TestBook = *Test;
	}
	return Parts.Num() > 0;
}

// ---------------------------------------------------------------------------------------------------------------
// Building it.

bool AAirStage::Raise(const FString& Key, const FVector2D& Aim, double Turn, const FVector2D& Target, double InUnitsPerMetre, FBattleEffects* InEffects)
{
	// A second Raise on one actor (another battle, another stage) first takes down what the first one put up.
	for (TObjectPtr<UTheaterStillMesh>& Mesh : StillMeshes)
	{
		if (Mesh)
		{
			Mesh->DestroyComponent();
		}
	}
	StillMeshes.Reset();
	for (TObjectPtr<UProceduralMeshComponent>& Mesh : ShipMeshes)
	{
		if (Mesh)
		{
			Mesh->DestroyComponent();
		}
	}
	ShipMeshes.Reset();
	Ships.Reset();
	OilLit.Reset();
	TanksAlight.Reset();
	// (each stage its own run of looks: the same strike on the same stage looks the same each time it is played)
	Looks.Begin(Fates::NameKey(std::string(TCHAR_TO_UTF8(*Key))) ^ 238u);
	bRaised = false;
	StageKey = Key;
	StageAim = Aim;
	StageTurn = Turn;
	BoardTarget = Target;
	UnitsPerMetre = FMath::Max(InUnitsPerMetre, 0.001);
	Effects = InEffects;
	if (!ReadStage(Key))
	{
		return false;
	}
	UMaterialInterface* Surface = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Campaign/Materials/M_MapSurface.M_MapSurface"));
	if (!Surface)
	{
		UE_LOG(LogTheater, Warning, TEXT("Stage: M_MapSurface is missing (Tools/build_content.py makes it); the default material stands in."));
		Surface = UMaterial::GetDefaultMaterial(MD_Surface);
	}
	auto Ink = [this, Surface](float Roughness, float Specular)
	{
		UMaterialInstanceDynamic* Made = UMaterialInstanceDynamic::Create(Surface, this);
		Made->SetScalarParameterValue(TEXT("Roughness"), Roughness);
		Made->SetScalarParameterValue(TEXT("Specular"), Specular);
		Made->SetScalarParameterValue(TEXT("Grain"), 0.0f);
		Made->SetScalarParameterValue(TEXT("Tooth"), 0.0f);
		return Made;
	};
	GroundInk = Ink(0.9f, 0.15f);
	// The ground's own grain, finer than its points are apart: the campaign map's noise at two sizes, one tile to
	// 1,800 m and one to 260 m of the stage (AirGround paints the broader patches point by point; the art director's
	// mottle at two scales on build 1734). -TheaterGroundGrain= tunes it, 0 for none.
	float GroundGrain = 0.28f;
	FParse::Value(FCommandLine::Get(), TEXT("TheaterGroundGrain="), GroundGrain);
	GroundInk->SetScalarParameterValue(TEXT("Grain"), FMath::Clamp(GroundGrain, 0.0f, 1.0f));
	GroundInk->SetScalarParameterValue(TEXT("GrainTilingCoarse"), static_cast<float>(1.0 / (1800.0 * UnitsPerMetre)));
	GroundInk->SetScalarParameterValue(TEXT("GrainTilingFine"), static_cast<float>(1.0 / (260.0 * UnitsPerMetre)));
	// (the shallows and the reef take no grain: their open-sea edge is painted the sea's colour to vanish into it, and
	// the sea has none; the cloud pre-check on round 6)
	WaterInk = Ink(0.9f, 0.15f);
	SeaInk = Ink(0.2f, 0.5f); // (a tighter sun path on the water)
	SteelInk = Ink(0.55f, 0.4f);
	// Land that lies past LandReach from the raid's aim is left out, its ground with it (the flight core and the clouds
	// have the sea there): a strike on a fleet is fought at sea. Midway's atoll, 38 km from the fleet's aim, showed
	// through the haze as a pale flat ring a quarter of the picture wide (the art director on build 1756's Midway 05:
	// the carriers were fought far out at sea, beyond sight of the island; open ocean is the picture).
	double LandOut = TNumericLimits<double>::Max();
	for (const FMeshPart& Part : Parts)
	{
		for (const FVector& Point : Part.Points)
		{
			LandOut = FMath::Min(LandOut, FVector2D::Distance(FVector2D(Point.X, Point.Y), Aim));
		}
	}
	const bool bLand = LandOut <= LandReach;
	// (the moving sea where it has been built: the open sea and the painted shallows and reef over it alike)
	if (UMaterialInstanceDynamic* Moving = MovingSea(bLand))
	{
		SeaInk = Moving;
		WaterInk = Moving;
	}
	BuildSea();
	if (bLand)
	{
		BuildGround();
		BuildWorks();
	}
	else
	{
		GridCols = GridRows = 0;
		GridZ.Reset();
	}
	bRaised = true;
	const FString LeftOut = bLand ? FString() : FString::Printf(TEXT("; its land left out, %.1f km from the aim (past %.0f km: a fight at sea)"), LandOut / 1000.0, LandReach / 1000.0);
	UE_LOG(LogTheater, Display, TEXT("Stage: %s raised, %d meshes, %d tanks, %d hangars, %d yard blocks, %d palms, the ground on a %d by %d grid%s."),
		*Key, Parts.Num(), Tanks.Num(), Hangars.Num(), Yard.Num(), Palms.Num(), GridCols, GridRows, *LeftOut);
	return true;
}

FVector2D AAirStage::ShoreAt(double East, double North) const
{
	return FVector2D((East - ShoreWest) / ShoreSpan, (North - ShoreSouth) / ShoreSpan);
}

UMaterialInstanceDynamic* AAirStage::MovingSea(bool bShore)
{
	// THE MOVING SEA (Brian, 21 November 05:53Z: "make the water more detailed in the pearl harbor air battle scene").
	// M_StageSea keeps the mesh's colours and lays over them six reads of the ripples picture, at three sizes (a swell,
	// a chop, ripples), each twice at its own heading off the trade wind, travelling downwind at the speed of its waves,
	// so the sun's path breaks into moving glitter; over them the wind's gusts in broad patches, whitecaps on the open
	// sea, and the harbour calm (Tools/build_content.py, build_stage_sea).
	static const bool bFlat = FParse::Param(FCommandLine::Get(), TEXT("TheaterFlatSea"));
	if (bFlat)
	{
		return nullptr;
	}
	UMaterialInterface* Moving = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Campaign/Materials/M_StageSea.M_StageSea"));
	if (!Moving)
	{
		UE_LOG(LogTheater, Warning, TEXT("Stage: M_StageSea is missing (run Tools/build_content.sh once to make it); the sea is drawn still."));
		return nullptr;
	}
	if (!SeaRipples)
	{
		TArray<FColor> Pixels;
		CampaignTextures::MakeWaterBumps(Pixels);
		SeaRipples = CampaignTextures::Create(CampaignTextures::DetailSize, CampaignTextures::DetailSize, Pixels, false, true,
			MakeUniqueObjectName(GetTransientPackage(), UTexture2D::StaticClass(), TEXT("StageSeaRipples")));
	}
	UMaterialInstanceDynamic* Made = UMaterialInstanceDynamic::Create(Moving, this);
	Made->SetTextureParameterValue(TEXT("Ripples"), SeaRipples);
	// (-TheaterSeaWaves= scales how much the waves lean the water: 0 is a still sheet, 1 as made)
	float Strength = 1.0f;
	FParse::Value(FCommandLine::Get(), TEXT("TheaterSeaWaves="), Strength);
	const double WorldPerMetre = UnitsPerMetre * FMath::Max(GetActorScale3D().X, 0.0001);
	// (downwind, as BuildSea lays the trade wind's streaks, turned into the world's frame)
	const double Downwind = FMath::DegreesToRadians(HeadingToYaw(FMath::DegreesToRadians(250.0) - StageTurn));
	const FVector Way = GetActorTransform().TransformVectorNoScale(FVector(FMath::Cos(Downwind), FMath::Sin(Downwind), 0.0)).GetSafeNormal2D();
	const double Base = FMath::Atan2(Way.Y, Way.X);
	// Each read: the picture's length in metres (its slopes run from a seventh of it down to a fifty-sixth); its
	// heading off the wind, degrees; how fast it travels, metres a second, about the speed of waves a seventh of its
	// length long (the square root of g L over two pi); how much it leans the water. The trade wind's sea in the lee
	// of Oahu: a low swell, a lively chop, and ripples over all.
	struct FRead
	{
		const TCHAR* Name;
		double Length, Off, Speed, Lean;
	};
	static const FRead Reads[] = {
		{ TEXT("SwellA"), 320.0, 0.0, 8.4, 0.10 }, { TEXT("SwellB"), 410.0, 14.0, 9.5, 0.08 },
		{ TEXT("ChopA"), 64.0, 25.0, 3.8, 0.11 }, { TEXT("ChopB"), 80.0, -18.0, 4.2, 0.09 },
		{ TEXT("RippleA"), 13.0, -40.0, 1.7, 0.08 }, { TEXT("RippleB"), 17.0, 55.0, 1.9, 0.07 },
	};
	for (const FRead& Read : Reads)
	{
		const double Heading = Base + FMath::DegreesToRadians(Read.Off);
		const double Across = Read.Length * WorldPerMetre;
		Made->SetVectorParameterValue(Read.Name, FLinearColor(static_cast<float>(FMath::Cos(Heading) / Across), static_cast<float>(FMath::Sin(Heading) / Across),
			static_cast<float>(Read.Speed / Read.Length), static_cast<float>(Read.Lean * FMath::Max(Strength, 0.0f))));
	}
	// (past about 6 km the waves are finer than the picture's pixels: they fold into a rougher, broader sun path)
	Made->SetScalarParameterValue(TEXT("FarDepth"), static_cast<float>(6000.0 * WorldPerMetre));
	if (!SeaWaves)
	{
		TArray<FColor> Pixels;
		CampaignTextures::MakeWaves(Pixels);
		SeaWaves = CampaignTextures::Create(CampaignTextures::DetailSize, CampaignTextures::DetailSize, Pixels, false, true,
			MakeUniqueObjectName(GetTransientPackage(), UTexture2D::StaticClass(), TEXT("StageSeaWaves")));
	}
	Made->SetTextureParameterValue(TEXT("Waves"), SeaWaves);
	// A read of a picture laid along the wind: its length in metres, its heading off the wind, how fast it slides
	// downwind (metres a second) and how much it does.
	auto Slid = [&](const TCHAR* Name, double Length, double Off, double Speed, double Amount)
	{
		const double Heading = Base + FMath::DegreesToRadians(Off);
		const double Across = Length * WorldPerMetre;
		Made->SetVectorParameterValue(Name, FLinearColor(static_cast<float>(FMath::Cos(Heading) / Across), static_cast<float>(FMath::Sin(Heading) / Across),
			static_cast<float>(Speed / Length), static_cast<float>(Amount)));
	};
	// Cat's-paws and slicks: patches some hundreds of metres across the wind and four times as long along it, riding
	// the trade wind at about its own speed (15 knots). (The art director, 07:20Z: drawn out 3 to 5 times along the wind.)
	const double GustAcross = 2400.0, GustAlong = GustAcross * 4.0;
	Slid(TEXT("Gusts"), GustAlong, 0.0, 7.5, FMath::Clamp(Strength, 0.0f, 1.0f));
	Made->SetVectorParameterValue(TEXT("GustsAcross"), FLinearColor(static_cast<float>(-FMath::Sin(Base) / (GustAcross * WorldPerMetre)),
		static_cast<float>(FMath::Cos(Base) / (GustAcross * WorldPerMetre)), 0.0f, 0.0f));
	// (their edges broken into fingers some 50 to 200 m across, riding with them; the art director on v3, 08:07Z)
	Slid(TEXT("GustFingers"), 600.0, 0.0, 7.5, 0.6 * FMath::Clamp(Strength, 0.0f, 1.0f));
	// Whitecaps: the whitecaps' picture read twice, 160 and 190 m to it, drifting downwind with the water at about a
	// metre a second while each crest runs on through its foam (the art director's notes, 07:20Z); put out of step
	// from place to place by the waves picture's slow unevenness, 640 m to it, so its repeats do not break together;
	// from 400 m out drawn as specks, fully so 1.1 km further, and gone between 4 and 10 km.
	if (!SeaFoam)
	{
		const std::vector<uint8_t> Foam = SeaFace::MakeFoam(512, 160.0);
		TArray<FColor> Pixels;
		Pixels.SetNumUninitialized(512 * 512);
		for (int32 K = 0; K < Pixels.Num(); ++K)
		{
			Pixels[K] = FColor(Foam[K * 4], Foam[K * 4 + 1], Foam[K * 4 + 2], Foam[K * 4 + 3]);
		}
		SeaFoam = CampaignTextures::Create(512, 512, Pixels, false, true, MakeUniqueObjectName(GetTransientPackage(), UTexture2D::StaticClass(), TEXT("StageSeaFoam")));
	}
	Made->SetTextureParameterValue(TEXT("Foam"), SeaFoam);
	const double Caps = FMath::Clamp(Strength, 0.0f, 1.5f);
	Slid(TEXT("FoamA"), 160.0, 0.0, 0.8, Caps);
	Slid(TEXT("FoamB"), 190.0, 5.0, 1.0, Caps);
	Slid(TEXT("FoamShift"), 640.0, 30.0, 0.3, 4.0);
	Made->SetScalarParameterValue(TEXT("FoamNear"), static_cast<float>(400.0 * WorldPerMetre));
	Made->SetScalarParameterValue(TEXT("FoamSpan"), static_cast<float>(1100.0 * WorldPerMetre));
	Made->SetScalarParameterValue(TEXT("FoamFadeFrom"), static_cast<float>(4000.0 * WorldPerMetre));
	Made->SetScalarParameterValue(TEXT("FoamFadeSpan"), static_cast<float>(6000.0 * WorldPerMetre));
	// The surf's sections: the waves picture's slow unevenness and swell, 520 m to it, coming in at 2 m a second.
	Slid(TEXT("SurfRead"), 520.0, 0.0, 2.0, 0.9 * FMath::Min(Caps, 1.2));
	// The coral heads on a reef flat or in a lagoon: the waves picture's slow unevenness, 900 m to it, lying still.
	Slid(TEXT("CoralRead"), 900.0, 0.0, 0.0, 1.0);
	// The shore (SeaFace::MakeShore), from the stage's land, reef and works, worked out once each time the stage is
	// raised (half a second or so). Where the land is not drawn, the open sea everywhere. Where each point of the sea
	// and the shallows falls on it goes with the point itself (ShoreAt, their first texture coordinate, laid in
	// BuildSea and BuildGround), worked out here in doubles: on the Mac the material's own reckoning from the world's
	// position put Pearl Harbor's lochs on the open sea.
	std::vector<SeaFace::Part> Hard;
	if (bShore)
	{
		for (const FMeshPart& Part : Parts)
		{
			if (Part.Name != TEXT("land") && Part.Name != TEXT("reef") && Part.Name != TEXT("works"))
			{
				continue;
			}
			SeaFace::Part& Into = Hard.emplace_back();
			Into.X.reserve(Part.Points.Num());
			Into.Y.reserve(Part.Points.Num());
			for (const FVector& Point : Part.Points)
			{
				Into.X.push_back(Point.X);
				Into.Y.push_back(Point.Y);
			}
			Into.Triangles.assign(Part.Triangles.GetData(), Part.Triangles.GetData() + Part.Triangles.Num());
		}
	}
	const double Began = FPlatformTime::Seconds();
	const SeaFace::Shore Shore = SeaFace::MakeShore(Hard, 70.0);
	TArray<FColor> ShorePixels;
	ShorePixels.SetNumUninitialized(Shore.Size * Shore.Size);
	for (int32 K = 0; K < ShorePixels.Num(); ++K)
	{
		ShorePixels[K] = FColor(Shore.Pixels[K * 4], Shore.Pixels[K * 4 + 1], Shore.Pixels[K * 4 + 2], Shore.Pixels[K * 4 + 3]);
	}
	SeaShore = CampaignTextures::Create(Shore.Size, Shore.Size, ShorePixels, false, false, MakeUniqueObjectName(GetTransientPackage(), UTexture2D::StaticClass(), TEXT("StageSeaShore")));
	Made->SetTextureParameterValue(TEXT("Shore"), SeaShore);
	ShoreWest = Shore.West;
	ShoreSouth = Shore.South;
	ShoreSpan = FMath::Max(Shore.Span, 1.0);
	UE_LOG(LogTheater, Display, TEXT("Stage: the sea's shore picture, %d a side over %.1f km, worked out in %.0f ms."), Shore.Size, Shore.Span / 1000.0,
		(FPlatformTime::Seconds() - Began) * 1000.0);
	return Made;
}

void AAirStage::BuildSea()
{
	// The open sea, under the island and its painted shallows. Round the battle, 48 km a side, it is a grid of 250 m
	// squares whose colour wanders in patches drawn out along the trade wind (cat's-paws and slicks) and whose
	// surface tilts a few degrees, so the sun's path on the water breaks into glitter; past it, one plain sheet to
	// far beyond every edge. (The art director's note on the first Mac run: over open water the picture showed haze,
	// "no sea".)
	UTheaterStillMesh* Sea = UTheaterStillMesh::Make(this, TEXT("StageSea"));
	Sea->SetupAttachment(Root);
	Sea->bUseAsyncCooking = false;
	Sea->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Sea->SetCastShadow(false);
	Sea->RegisterComponent();
	const double U = UnitsPerMetre;
	const double Far = 200000.0 * U, Inner = 24000.0 * U, Cell = 250.0 * U;
	const FVector Middle = BoardToLocal(BoardTarget, 0.0);
	// (the trade wind from about 070, so its streaks run 070 to 250: a true heading, turned into the stage's own axes
	// as the sun's and the hangars' are)
	const double WindYaw = FMath::DegreesToRadians(HeadingToYaw(FMath::DegreesToRadians(250.0) - StageTurn));
	const FVector2D Along(FMath::Cos(WindYaw), FMath::Sin(WindYaw)), Across(-Along.Y, Along.X);
	auto Height = [&Along, &Across](double X, double Y)
	{
		// (metres; long patches 3 km by 1 km, and smaller ones 1.5 km by 600 m: no smaller than two of the grid's
		// squares across, or the grid draws them as speckle)
		const double A = (X * Along.X + Y * Along.Y), B = (X * Across.X + Y * Across.Y);
		return FMath::PerlinNoise2D(FVector2D(A / 3000.0, B / 1000.0)) + 0.45 * FMath::PerlinNoise2D(FVector2D(A / 1500.0 + 17.3, B / 600.0 - 4.1));
	};
	const FColor Paint = StageParts::SeaPaintUnder(SunElevation);
	const FLinearColor Deep(Paint);
	TArray<FVector> V;
	TArray<FVector> N;
	TArray<FColor> C;
	TArray<FVector2D> OnShore;
	TArray<int32> T;
	// (where a point of the sea, in this actor's frame, falls on the shore picture)
	auto Shored = [this](const FVector& Local)
	{
		const FVector2D Stage = BoardToStage(FVector2D(Local.Y, Local.X) / UnitsPerMetre);
		return ShoreAt(Stage.X, Stage.Y);
	};
	auto Lay = [&V, &T](int32 A, int32 B, int32 D)
	{
		// (each triangle the way round that faces up, as FMiniatureMesh::Face lays them)
		const FVector Facing = (V[B] - V[D]) ^ (V[A] - V[D]);
		T.Append(Facing.Z >= 0.0 ? TArray<int32>{ A, B, D } : TArray<int32>{ A, D, B });
	};
	const int32 Count = FMath::RoundToInt32(2.0 * Inner / Cell);
	for (int32 Row = 0; Row <= Count; ++Row)
	{
		for (int32 Col = 0; Col <= Count; ++Col)
		{
			const double X = -Inner + Col * Cell, Y = -Inner + Row * Cell;
			const double Mx = X / U, My = Y / U;
			// (the patches fade to the plain sea over the grid's last 4 km, so its edge does not show)
			const double Edge = FMath::Clamp((Inner / U - FMath::Max(FMath::Abs(Mx), FMath::Abs(My))) / 4000.0, 0.0, 1.0);
			const double H = Height(Mx, My) * Edge;
			const double Step = 0.5 * Cell / U;
			const double Dx = (Height(Mx + Step, My) - Height(Mx - Step, My)) * Edge / (2.0 * Step);
			const double Dy = (Height(Mx, My + Step) - Height(Mx, My - Step)) * Edge / (2.0 * Step);
			// (a tilt of up to some 5 degrees, as before the smaller patches were made broader and their slopes gentler)
			const double Tilt = 60.0;
			V.Add(Middle + FVector(X, Y, 0.0));
			OnShore.Add(Shored(V.Last()));
			N.Add(FVector(-Dx * Tilt, -Dy * Tilt, 1.0).GetSafeNormal());
			const float Shade = static_cast<float>(1.0 + 0.11 * H);
			C.Add(FLinearColor(Deep.R * Shade, Deep.G * Shade, Deep.B * Shade * 1.02f).ToFColor(true));
		}
	}
	for (int32 Row = 0; Row < Count; ++Row)
	{
		for (int32 Col = 0; Col < Count; ++Col)
		{
			const int32 A = Row * (Count + 1) + Col, B = A + 1, D = A + Count + 1, E = D + 1;
			Lay(A, B, E);
			Lay(A, E, D);
		}
	}
	// The plain sheet beyond, in four bands round the grid (never under it: two sheets at one height flicker).
	const FVector2D Bands[4][2] = {
		{ FVector2D(-Far, -Far), FVector2D(Far, -Inner) },
		{ FVector2D(-Far, Inner), FVector2D(Far, Far) },
		{ FVector2D(-Far, -Inner), FVector2D(-Inner, Inner) },
		{ FVector2D(Inner, -Inner), FVector2D(Far, Inner) },
	};
	for (const auto& Band : Bands)
	{
		const int32 First = V.Num();
		for (const FVector2D& Corner : { Band[0], FVector2D(Band[1].X, Band[0].Y), Band[1], FVector2D(Band[0].X, Band[1].Y) })
		{
			V.Add(Middle + FVector(Corner.X, Corner.Y, 0.0));
			OnShore.Add(Shored(V.Last()));
			N.Add(FVector::UpVector);
			C.Add(Paint);
		}
		Lay(First, First + 1, First + 2);
		Lay(First, First + 2, First + 3);
	}
	Sea->CreateMeshSection(0, V, T, N, OnShore, C, TArray<FProcMeshTangent>(), false);
	Sea->SetMaterial(0, SeaInk);
	StillMeshes.Add(Sea);
}

void AAirStage::BuildGround()
{
	// The ground as painted (AirGround, the same code the cloud test runs): the grass varied and here and there turned
	// toward red earth, the sand kept to the water's edge, the shallows darkening into the sea's own paint, the runways
	// lifted clear of the land. (The file's names are plain letters: copied across one by one.)
	auto Plain = [](const FString& Text)
	{
		std::string Out;
		Out.reserve(static_cast<size_t>(Text.Len()));
		for (int32 K = 0; K < Text.Len(); ++K)
		{
			Out.push_back(static_cast<char>(Text[K]));
		}
		return Out;
	};
	std::vector<AirGround::Part> Given;
	Given.reserve(static_cast<size_t>(Parts.Num()));
	for (const FMeshPart& Part : Parts)
	{
		AirGround::Part& Into = Given.emplace_back();
		Into.Name = Plain(Part.Name);
		Into.X.reserve(static_cast<size_t>(Part.Points.Num()));
		Into.Y.reserve(static_cast<size_t>(Part.Points.Num()));
		Into.Z.reserve(static_cast<size_t>(Part.Points.Num()));
		for (const FVector& Point : Part.Points)
		{
			Into.X.push_back(Point.X);
			Into.Y.push_back(Point.Y);
			Into.Z.push_back(Point.Z);
		}
		Into.Paints.assign(Part.Paints.GetData(), Part.Paints.GetData() + Part.Paints.Num());
		Into.Tris.assign(Part.Triangles.GetData(), Part.Triangles.GetData() + Part.Triangles.Num());
	}
	std::vector<std::string> Names;
	std::vector<AirGround::Rgb8> Colours;
	for (int32 K = 0; K < Palette.Num(); ++K)
	{
		Names.push_back(PaletteNames.IsValidIndex(K) ? Plain(PaletteNames[K]) : std::string());
		Colours.push_back({ Palette[K].R, Palette[K].G, Palette[K].B });
	}
	const FColor SeaNow = StageParts::SeaPaintUnder(SunElevation);
	AirGround::Tally Count;
	// (split finer within 4 km of the battle's aim, so the mottle and the shallows' fade show no fans there)
	AirGround::Focus Near;
	Near.X = StageAim.X;
	Near.Y = StageAim.Y;
	Near.Radius = 4000.0;
	const std::vector<AirGround::Drawn> Painted = AirGround::Paint(Given, Names, Colours, { SeaNow.R, SeaNow.G, SeaNow.B }, &Count, Near);
	UE_LOG(LogTheater, Display, TEXT("Stage: the ground painted, %d points laid near the battle, %d sand points laid, shallows %d faded and %d kept%s, %d sheltered shore points, %d earth, %d runway points lifted."),
		Count.Refined, Count.SandSplit, Count.ShoreFaded, Count.ShoreKept, Count.bLagoon ? TEXT(" (a lagoon: the edge kept)") : TEXT(""), Count.Sheltered, Count.Earth, Count.RunwayRaised);

	for (const AirGround::Drawn& Part : Painted)
	{
		const FString PartName(Part.Name.c_str());
		UTheaterStillMesh* Mesh = UTheaterStillMesh::Make(this, *(TEXT("Stage_") + PartName));
		Mesh->SetupAttachment(Root);
		Mesh->bUseAsyncCooking = false;
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		// (the hills and the works cast shadows; what is painted on the water does not)
		Mesh->SetCastShadow(PartName == TEXT("land") || PartName == TEXT("works"));
		Mesh->RegisterComponent();
		const int32 Points = static_cast<int32>(FMath::Min(Part.X.size(), Part.Colours.size()));
		// (the shallows and the reef wear the moving sea, which reads the shore picture where each point falls on it)
		const bool bWater = PartName == TEXT("shallows") || PartName == TEXT("reef");
		TArray<FVector> V;
		TArray<FColor> C;
		TArray<FVector2D> OnShore;
		V.Reserve(Points);
		C.Reserve(Points);
		for (int32 K = 0; K < Points; ++K)
		{
			const size_t I = static_cast<size_t>(K);
			V.Add(StageToLocal(FVector(Part.X[I], Part.Y[I], Part.Z[I])));
			C.Add(FColor(Part.Colours[I].R, Part.Colours[I].G, Part.Colours[I].B));
			if (bWater)
			{
				OnShore.Add(ShoreAt(Part.X[I], Part.Y[I]));
			}
		}
		TArray<FVector> N;
		N.Init(FVector::ZeroVector, V.Num());
		TArray<int32> T;
		T.Reserve(static_cast<int32>(Part.Tris.size()));
		for (size_t K = 0; K + 2 < Part.Tris.size(); K += 3)
		{
			const int32 A = Part.Tris[K], B = Part.Tris[K + 1], D = Part.Tris[K + 2];
			if (!V.IsValidIndex(A) || !V.IsValidIndex(B) || !V.IsValidIndex(D))
			{
				continue;
			}
			const FVector Facing = (V[B] - V[D]) ^ (V[A] - V[D]);
			const double Size = Facing.Size();
			if (Size <= UE_DOUBLE_SMALL_NUMBER)
			{
				continue;
			}
			if (FMath::Abs(Facing.Z) < 0.2 * Size)
			{
				// A wall (a pier's side): laid both ways round, so it shows from either side.
				T.Append({ A, B, D, A, D, B });
				continue;
			}
			// (the side that faces up is the side drawn: FMiniatureMesh::Face's rule)
			const FVector Up = Facing.Z >= 0.0 ? Facing : -Facing;
			T.Append(Facing.Z >= 0.0 ? TArray<int32>{ A, B, D } : TArray<int32>{ A, D, B });
			N[A] += Up;
			N[B] += Up;
			N[D] += Up;
		}
		for (FVector& Normal : N)
		{
			Normal = Normal.IsNearlyZero() ? FVector::UpVector : Normal.GetSafeNormal();
		}
		Mesh->CreateMeshSection(0, V, T, N, OnShore, C, TArray<FProcMeshTangent>(), false);
		Mesh->SetMaterial(0, bWater ? WaterInk : GroundInk);
		StillMeshes.Add(Mesh);
	}
}

void AAirStage::BuildWorks()
{
	// The oil tanks, the hangars, the yard's blocks and the palms: built with the miniature kit, in one mesh.
	const double U = UnitsPerMetre;
	FMiniatureMesh Model;
	for (const TArray<double>& Tank : Tanks)
	{
		if (Tank.Num() < 5)
		{
			continue;
		}
		const FVector Foot = StageToLocal(FVector(Tank[0], Tank[1], Tank[2]));
		Model.Pillar(Foot, Tank[3] * U, Tank[3] * U, Tank[4] * U, StageParts::TankPaint, 20);
		Model.Disc(Foot + FVector(0.0, 0.0, Tank[4] * U), FVector::UpVector, Tank[3] * U, StageParts::TankPaint, 20);
	}
	for (const TArray<double>& Hangar : Hangars)
	{
		if (Hangar.Num() < 7)
		{
			continue;
		}
		// x, y, ground, length along the runway, width, height, the runway's compass heading
		const FVector Foot = StageToLocal(FVector(Hangar[0], Hangar[1], Hangar[2]));
		const double Yaw = HeadingToYaw(FMath::DegreesToRadians(Hangar[6]) - StageTurn);
		Model.Box(Foot + FVector(0.0, 0.0, Hangar[5] * 0.4 * U), FVector(Hangar[3], Hangar[4], Hangar[5] * 0.8) * U, StageParts::HangarPaint, Yaw);
		Model.Box(Foot + FVector(0.0, 0.0, Hangar[5] * 0.9 * U), FVector(Hangar[3], Hangar[4] * 0.8, Hangar[5] * 0.2) * U, StageParts::RoofPaint, Yaw);
	}
	for (const TArray<double>& Block : Yard)
	{
		if (Block.Num() < 8)
		{
			continue;
		}
		// The historian's 1941 yard (tools/stages/yard.py): x, y, ground, length, width, height, the length's compass
		// heading, kind. The ground is the highest land under the block, and each piece goes 4.5 m down, so the land
		// falling away under it (3.4 m at most at Pearl Harbor) leaves no gap (the cloud pre-check on round 6).
		const FVector Foot = StageToLocal(FVector(Block[0], Block[1], Block[2]));
		const double Yaw = HeadingToYaw(FMath::DegreesToRadians(Block[6]) - StageTurn);
		const double Long = Block[3], Wide = Block[4], High = Block[5];
		const FQuat Turned = FQuat(FRotator(0.0, Yaw, 0.0));
		const FVector Along = Turned.RotateVector(FVector::ForwardVector), Across = Turned.RotateVector(FVector::RightVector);
		// (a box from Low to Top metres about the ground, Ahead along the length and Aside across it)
		auto Slab = [&Model, &Foot, &Along, &Across, Yaw, U](double Ahead, double Aside, double Length, double Width, double Low, double Top, const FLinearColor& Paint)
		{
			Model.Box(Foot + (Along * Ahead + Across * Aside) * U + FVector(0.0, 0.0, (Low + Top) * 0.5 * U), FVector(Length, Width, Top - Low) * U, Paint, Yaw);
		};
		// (a paved top 0.3 m up as slabs some 10 m square, each its own shade of worn concrete, over a box 5 cm lower:
		// the art director's fourth note of 17:45Z; AirGround::Paving, which the cloud test draws too)
		auto Paved = [this, &Model, &Block, &Slab](double PadLong, double PadWide)
		{
			Slab(0.0, 0.0, PadLong, PadWide, StageParts::YardSkirt, 0.25, StageParts::ConcretePaint);
			const FColor Grey = StageParts::ConcretePaint.ToFColor(true);
			const double Compass = FMath::DegreesToRadians(Block[6]);
			const FVector2D LengthWay(FMath::Sin(Compass), FMath::Cos(Compass)), WidthWay(FMath::Cos(Compass), -FMath::Sin(Compass));
			const double PavedAt = Block[2] + 0.3;
			for (const AirGround::Panel& Square : AirGround::Paving(Block[0], Block[1], PadLong, PadWide, Block[6], { Grey.R, Grey.G, Grey.B }))
			{
				const FVector2D Middle(Square.X, Square.Y), HalfLong = LengthWay * (Square.Long * 0.5), HalfWide = WidthWay * (Square.Wide * 0.5);
				const FVector P0 = StageToLocal(FVector(Middle - HalfLong - HalfWide, PavedAt)), P1 = StageToLocal(FVector(Middle + HalfLong - HalfWide, PavedAt));
				const FVector P2 = StageToLocal(FVector(Middle + HalfLong + HalfWide, PavedAt)), P3 = StageToLocal(FVector(Middle - HalfLong + HalfWide, PavedAt));
				Model.Quad(P0, P1, P2, P3, FVector::UpVector, FColor(Square.Colour.R, Square.Colour.G, Square.Colour.B));
			}
		};
		switch (static_cast<int32>(Block[7]))
		{
		case 0: // a shop or a hangar on its pad
			Paved(Long + 24.0, Wide + 24.0);
			Slab(0.0, 0.0, Long, Wide, -1.0, High * 0.8, StageParts::HangarPaint);
			Slab(0.0, 0.0, Long, Wide * 0.8, High * 0.8, High, StageParts::RoofPaint);
			break;
		case 1: // a dry dock, drained: its floor dark inside the coping, the caisson the end on the heading's side
			Slab(0.0, 0.0, Long - 20.0, Wide - 15.0, StageParts::YardSkirt, 0.2, StageParts::DockFloorPaint);
			Slab(-(Long * 0.5 - 5.0), 0.0, 10.0, Wide, StageParts::YardSkirt, 0.6, StageParts::ConcretePaint);
			Slab(Long * 0.5 - 5.0, 0.0, 10.0, Wide, StageParts::YardSkirt, 0.8, StageParts::DockSteelPaint);
			Slab(0.0, -(Wide * 0.5 - 3.75), Long - 20.0, 7.5, StageParts::YardSkirt, 0.6, StageParts::ConcretePaint);
			Slab(0.0, Wide * 0.5 - 3.75, Long - 20.0, 7.5, StageParts::YardSkirt, 0.6, StageParts::ConcretePaint);
			break;
		case 2: // a paved apron
			Paved(Long, Wide);
			break;
		case 3: // a floating dry dock: the pontoon awash, the wing walls either side
			Slab(0.0, 0.0, Long, Wide, -2.0, 1.5, StageParts::DockSteelPaint);
			Slab(0.0, -(Wide * 0.5 - 1.5), Long, 3.0, 1.5, FMath::Max(High, 2.0), StageParts::DockSteelPaint);
			Slab(0.0, Wide * 0.5 - 1.5, Long, 3.0, 1.5, FMath::Max(High, 2.0), StageParts::DockSteelPaint);
			break;
		case 4: // a mooring quay standing in the water, its top the height above the sea
			Slab(0.0, 0.0, Long, Wide, -0.5, FMath::Max(High, 0.5), StageParts::ConcretePaint);
			break;
		default:
			break;
		}
	}
	FRandomStream Lean(Palms.Num());
	for (const TArray<double>& Palm : Palms)
	{
		if (Palm.Num() < 4)
		{
			continue;
		}
		const FVector Foot = StageToLocal(FVector(Palm[0], Palm[1], Palm[2]));
		const double High = Palm[3], Turn = Lean.FRandRange(0.0f, 360.0f);
		// (the Asia thread's coconut palms where they are among the game's models, far detail: 375 triangles each;
		// young 12.5 m, grown 19.5 m, tall shore palms 25 m, each scaled 0.85 to 1.15 to the height asked)
		const TCHAR* Kind = High < 16.0 ? TEXT("CoconutPalmYoung") : High < 22.0 ? TEXT("CoconutPalm") : TEXT("CoconutPalmTall");
		const double Grown = High < 16.0 ? 12.5 : High < 22.0 ? 19.5 : 25.0;
		if (Model.Sculpted(Kind, FString(), Foot, U * FMath::Clamp(High / Grown, 0.85, 1.15), Turn, true))
		{
			continue;
		}
		const FVector Way = FRotator(0.0, Turn, 0.0).Vector();
		const FVector Top = Foot + Way * (High * 0.12 * U) + FVector(0.0, 0.0, High * U);
		Model.Limb(Foot, Top, 0.24 * U, 0.16 * U, StageParts::TrunkPaint, 6);
		for (int32 K = 0; K < 7; ++K)
		{
			const FVector Out = FRotator(0.0, Turn + K * 360.0 / 7.0, 0.0).Vector();
			Model.Limb(Top, Top + Out * (High * 0.28 * U) - FVector(0.0, 0.0, High * 0.12 * U), 0.5 * U, 0.08 * U, StageParts::FrondPaint, 4);
		}
	}
	if (Model.Vertices.Num() == 0)
	{
		return;
	}
	UTheaterStillMesh* Mesh = UTheaterStillMesh::Make(this, TEXT("StageWorks"));
	Mesh->SetupAttachment(Root);
	Mesh->bUseAsyncCooking = false;
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(true);
	Mesh->RegisterComponent();
	Mesh->CreateMeshSection(0, Model.Vertices, Model.Triangles, Model.Normals, TArray<FVector2D>(), Model.Colors, TArray<FProcMeshTangent>(), false);
	Mesh->SetMaterial(0, GroundInk);
	StillMeshes.Add(Mesh);
}

// ---------------------------------------------------------------------------------------------------------------
// The ships.

void AAirStage::SetShips(const TArray<FStageShip>& Given)
{
	for (const FStageShip& Ship : Given)
	{
		LayShip(Ship);
	}
}

void AAirStage::SetShipsFromView(const TSharedPtr<FJsonObject>& ShipsView)
{
	const TArray<TSharedPtr<FJsonValue>>* Pieces = nullptr;
	if (!ShipsView.IsValid() || !ShipsView->TryGetArrayField(TEXT("pieces"), Pieces))
	{
		return;
	}
	TArray<FStageShip> Given;
	for (const TSharedPtr<FJsonValue>& Item : *Pieces)
	{
		const TSharedPtr<FJsonObject>* Piece = nullptr;
		if (!Item->TryGetObject(Piece))
		{
			continue;
		}
		FStageShip& Ship = Given.AddDefaulted_GetRef();
		(*Piece)->TryGetNumberField(TEXT("id"), Ship.Id);
		(*Piece)->TryGetStringField(TEXT("cls"), Ship.Class);
		(*Piece)->TryGetStringField(TEXT("nat"), Ship.Nation);
		(*Piece)->TryGetStringField(TEXT("name"), Ship.Name); // (null in the view for an unnamed ship: left empty)
		(*Piece)->TryGetNumberField(TEXT("x"), Ship.At.X);
		(*Piece)->TryGetNumberField(TEXT("y"), Ship.At.Y);
		(*Piece)->TryGetNumberField(TEXT("face"), Ship.Face);
		(*Piece)->TryGetBoolField(TEXT("dock"), Ship.bDock);
	}
	SetShips(Given);
}

void AAirStage::LayShip(const FStageShip& Given)
{
	if (const FShip* Going = Ships.Find(Given.Id))
	{
		if (Going->SunkAt >= 0.0 || Going->bGone)
		{
			// (a ship already going down is Advance's to move: laid again she would pop back to the waterline)
			return;
		}
	}
	FShip& Ship = Ships.FindOrAdd(Given.Id);
	Ship.Given = Given;
	{
		// (a ship in a dock stands on its floor, not in the sea under it: the cloud pre-check on round 6)
		const double Floor = DockFloor(BoardToStage(Given.At));
		Ship.Base = BoardToLocal(Given.At, Floor);
		Ship.bDocked = Floor > 0.0;
	}
	// (a model is built with its bow toward -X: turned half round more, the bow lies along her heading)
	Ship.Yaw = HeadingToYaw(Given.Face) + 180.0;
	{
		// In harbour if there is land within two kilometres of her: she settles on the bottom there, and at sea goes down.
		const FVector2D Stage = BoardToStage(Given.At);
		Ship.bHarbour = false;
		const int32 Reach = GridCell > 0.0 ? FMath::CeilToInt(2000.0 / GridCell) : 0;
		const int32 C0 = FMath::FloorToInt((Stage.X - GridE0) / FMath::Max(GridCell, 1.0)), R0 = FMath::FloorToInt((Stage.Y - GridN0) / FMath::Max(GridCell, 1.0));
		for (int32 R = R0 - Reach; R <= R0 + Reach && !Ship.bHarbour; ++R)
		{
			for (int32 C = C0 - Reach; C <= C0 + Reach; ++C)
			{
				if (R >= 0 && C >= 0 && R < GridRows && C < GridCols && GridZ[R * GridCols + C] > 0)
				{
					Ship.bHarbour = true;
					break;
				}
			}
		}
	}
	if (!Ship.Hull)
	{
		FMiniatureMesh Model;
		const FString Name = FString(StageParts::KindOf(Given.Class)) + TEXT(":") + Given.Nation;
		{
			TGuardValue<uint8> Painted(Model.Finish, 128); // (ships are painted metal)
			if (!Model.Vehicle(Name, FVector::ZeroVector, UnitsPerMetre, 0.0) || Model.Vertices.Num() == 0)
			{
				UE_LOG(LogTheater, Warning, TEXT("Stage: no model for %s; ship %d is not drawn."), *Name, Given.Id);
				Ships.Remove(Given.Id);
				return;
			}
		}
		const int32 Repainted = StageParts::InStagePaint(Model, Given.Nation);
		static bool bRepaintTold = false;
		if (Repainted > 0 && !bRepaintTold)
		{
			bRepaintTold = true;
			UE_LOG(LogTheater, Display, TEXT("Stage: %s in Measure 1, Dark Gray 5-D with her decks Deck Blue 20-B (%d of %d points repainted)."), *Name, Repainted, Model.Colors.Num());
		}
		StageParts::FitHull(Model, Given, UnitsPerMetre);
		FBox Bounds(ForceInit);
		for (const FVector& Point : Model.Vertices)
		{
			Bounds += Point;
		}
		const FVector Size = Bounds.GetSize() / UnitsPerMetre;
		Ship.Long = FMath::Max(Size.X, 10.0);
		Ship.Beam = FMath::Max(Size.Y, 4.0);
		Ship.High = FMath::Max(Bounds.Max.Z / UnitsPerMetre, 3.0);
		UProceduralMeshComponent* Hull = NewObject<UProceduralMeshComponent>(this);
		Hull->SetupAttachment(Root);
		Hull->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Hull->SetCastShadow(true);
		Hull->RegisterComponent();
		Hull->CreateMeshSection(0, Model.Vertices, Model.Triangles, Model.Normals, TArray<FVector2D>(), Model.Colors, TArray<FProcMeshTangent>(), false);
		Hull->SetMaterial(0, SteelInk);
		Ship.Hull = Hull;
		ShipMeshes.Add(Hull);
	}
	Ship.Hull->SetRelativeLocationAndRotation(Ship.Base, FRotator(0.0, Ship.Yaw, 0.0));
}

void AAirStage::Hit(int32 Id, const FString& Strike, double Now)
{
	FShip* Ship = Ships.Find(Id);
	if (!Ship || Ship->bGone)
	{
		return;
	}
	Ship->Hits++;
	if (Ship->FirstHitAt < 0.0)
	{
		Ship->FirstHitAt = Now;
	}
	if (!Effects)
	{
		return;
	}
	if (Looks.IsReady())
	{
		// Each hit its own look (Battle/Fates): a fire put out or spreading, a torpedo's flooding and list, steam, a
		// bomb bursting on armour, a carrier's deck park going up. Hurt: each hit she has taken counts a quarter.
		Looks.Hit(Id, Ship->Given.Class, Strike, Ship->bHarbour, (Ship->Hits - 1) * 0.25, Now);
		return;
	}
	const double U = UnitsPerMetre;
	const FRotator Turned(0.0, Ship->Yaw, 0.0);
	const FVector Along = Turned.RotateVector(FVector::ForwardVector), Abeam = Turned.RotateVector(FVector::RightVector);
	FVector Local = Ship->Base + Along * (Shown.FRandRange(-0.35f, 0.35f) * Ship->Long * U);
	if (Strike == TEXT("torpedo"))
	{
		// A torpedo: the column of water at her side, the side the run came from (the effects kit's torpedo, 22:59Z: the
		// tallest, narrowest column, over her mast, dark for its lower third).
		Local += Abeam * ((Ship->Beam * 0.5 + 3.0) * U * (Shown.FRand() < 0.5f ? 1.0 : -1.0));
		Effects->ShellOfSize(GetActorTransform().TransformPosition(Local), TEXT("torpedo"), TEXT("water"));
		return;
	}
	// A bomb on her deck: the dive bombers' 250 kg, the level bombers' heavier armour-piercing bombs.
	const double Deck = FMath::Min(Ship->High * 0.3, 9.0);
	Local.Z += Deck * U;
	const FVector Struck = GetActorTransform().TransformPosition(Local);
	Effects->ShellOfSize(Struck, Strike == TEXT("dive") ? TEXT("bomb250") : TEXT("bomb500"), TEXT("town"), true);
	if (Shown.FRand() < (Strike == TEXT("dive") ? 0.5f : 0.3f))
	{
		// (-1, true, here and at every fire on a ship: no id, and afloat, so the kit burns it as a ship's fire, never
		// as a burning tank's however small or low it is: the effects kit's Burn bOnShip, 08a18f21)
		Effects->Burn(Struck, static_cast<float>(Ship->Beam * 0.5), 240.0f, -1, true);
	}
}

void AAirStage::Blast(FShip& Ship, bool bBig)
{
	if (!Effects)
	{
		return;
	}
	// The magazine forward: one blast on another, the biggest of the battle bigger again (ruling 238(5)).
	const double U = UnitsPerMetre;
	const FRotator Turned(0.0, Ship.Yaw, 0.0);
	const FVector Along = Turned.RotateVector(FVector::ForwardVector), Abeam = Turned.RotateVector(FVector::RightVector);
	// (her bow is toward the model's -X; her look may name the after magazine)
	const FVector Forward = Ship.Base - Along * (Ship.Long * (Ship.End == TEXT("aft") ? -0.25 : 0.25) * U);
	const int32 Blasts = bBig ? 7 : 3;
	for (int32 K = 0; K < Blasts; ++K)
	{
		const FVector Spot = Forward + FVector(Shown.FRandRange(-1.0f, 1.0f) * Ship.Beam * 0.4 * U, Shown.FRandRange(-1.0f, 1.0f) * Ship.Beam * 0.4 * U, (Ship.High * 0.2 + K * 3.0) * U);
		Effects->ShellOfSize(GetActorTransform().TransformPosition(Spot), TEXT("bomb500"), TEXT("water"), true);
	}
	// (and the sea thrown up beside the magazine: off armour, the effects kit draws a column of white water)
	Effects->ShellOfSize(GetActorTransform().TransformPosition(Forward + Abeam * ((Ship.Beam * 0.5 + 2.0) * (Shown.FRand() < 0.5f ? 1.0 : -1.0) * U)), TEXT("bomb500"), TEXT("water"), false);
	// (in harbour she settles on the mud and burns on; at sea she is under in about 30 s, and her fire goes with her,
	// her oil's column on the water burning on where she went down: the ship looks thread, 15:25Z)
	Effects->Burn(GetActorTransform().TransformPosition(Forward + FVector(0.0, 0.0, Ship.High * 0.2 * U)), static_cast<float>(Ship.Beam * (bBig ? 2.0 : 1.2)), Ship.bHarbour ? 1800.0f : 32.0f, -1, true);
}

void AAirStage::Sink(int32 Id, const FString& How, bool bBig, double Now)
{
	FShip* Ship = Ships.Find(Id);
	if (!Ship || !Ship->How.IsEmpty())
	{
		return;
	}
	Ship->How = How;
	Ship->SunkAt = Now;
	if (const Fates::Fate* Look = Looks.Sink(Id, Ship->Given.Class, How, Ship->bHarbour, Now))
	{
		// Her end's details from her look; she goes on toward the side she was flooding on, where she was.
		Ship->Side = Looks.SideOf(Id) != 0.0 ? Looks.SideOf(Id) : (Look->Dial("side", 1.0) < 0.0 ? -1.0 : 1.0);
		Ship->Pace = FMath::Clamp(Look->Dial("pace", 1.0), 0.5, 2.0);
		Ship->Roll = Look->Dial("roll", -1.0);
		Ship->Cut = FMath::Clamp(Look->Dial("cut", 0.0), -0.35, 0.35);
		Ship->End = UTF8_TO_TCHAR(Look->DialText("end").c_str());
	}
	const double U = UnitsPerMetre;
	const FRotator Turned(0.0, Ship->Yaw, 0.0);
	const FVector Along = Turned.RotateVector(FVector::ForwardVector);
	if (How == TEXT("explode"))
	{
		Blast(*Ship, bBig);
	}
	else if (How == TEXT("burn") && Effects)
	{
		// Ablaze from end to end: fires along her length that burn for the rest of the battle and after. Five small
		// fires, not three the width of her deck (the art director on build 1750's Akagi: three fires each 1.1 of her
		// beam sent up puffs 70 to 140 m across, a soft brown blur over her that hid the black column her burning oil
		// sends up; at under half her beam their flames still run her length and the column above her is Pearl
		// Harbor's, black at its foot, lumpy and leaning downwind).
		// (fires her hits have already lit count toward the five, FShipLooks::MostFires, but her end lights two at least:
		// her bow and her stern, so that she burns from end to end however her hits went)
		int32 Room = FMath::Max(2, FShipLooks::MostFires - Looks.FiresOn(Id, Now));
		for (const double Share : { -0.4, 0.4, 0.0, -0.2, 0.2 })
		{
			if (Room-- <= 0)
			{
				break;
			}
			const FVector Spot = Ship->Base + Along * (Share * Ship->Long * U) + FVector(0.0, 0.0, FMath::Min(Ship->High * 0.3, 9.0) * U);
			Effects->Burn(GetActorTransform().TransformPosition(Spot), static_cast<float>(Ship->Beam * 0.45), 1800.0f, -1, true);
			Looks.AddFire(Id, Now + 1800.0);
		}
	}
	else if (How == TEXT("break"))
	{
		// Broken in two amidships: her model cut in halves, bow and stern, each going its own way.
		FMiniatureMesh Model;
		{
			TGuardValue<uint8> Painted(Model.Finish, 128);
			Model.Vehicle(FString(StageParts::KindOf(Ship->Given.Class)) + TEXT(":") + Ship->Given.Nation, FVector::ZeroVector, UnitsPerMetre, 0.0);
		}
		StageParts::InStagePaint(Model, Ship->Given.Nation);
		StageParts::FitHull(Model, Ship->Given, UnitsPerMetre);
		TArray<int32> Stern, Bow;
		for (int32 K = 0; K + 2 < Model.Triangles.Num(); K += 3)
		{
			const double X = (Model.Vertices[Model.Triangles[K]].X + Model.Vertices[Model.Triangles[K + 1]].X + Model.Vertices[Model.Triangles[K + 2]].X) / 3.0;
			(X < Ship->Cut * Ship->Long * UnitsPerMetre ? Bow : Stern).Append({ Model.Triangles[K], Model.Triangles[K + 1], Model.Triangles[K + 2] });
		}
		if (Bow.Num() > 0 && Stern.Num() > 0 && Ship->Hull)
		{
			Ship->Hull->CreateMeshSection(0, Model.Vertices, Stern, Model.Normals, TArray<FVector2D>(), Model.Colors, TArray<FProcMeshTangent>(), false);
			UProceduralMeshComponent* Fore = NewObject<UProceduralMeshComponent>(this);
			Fore->SetupAttachment(Root);
			Fore->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Fore->SetCastShadow(true);
			Fore->RegisterComponent();
			Fore->CreateMeshSection(0, Model.Vertices, Bow, Model.Normals, TArray<FVector2D>(), Model.Colors, TArray<FProcMeshTangent>(), false);
			Fore->SetMaterial(0, SteelInk);
			Fore->SetRelativeLocationAndRotation(Ship->Base, FRotator(0.0, Ship->Yaw, 0.0));
			Ship->Fore = Fore;
			ShipMeshes.Add(Fore);
		}
		if (Effects)
		{
			Effects->ShellOfSize(GetActorTransform().TransformPosition(Ship->Base + FVector(0.0, 0.0, 3.0 * U)), TEXT("bomb500"), TEXT("water"), true);
			Effects->Burn(GetActorTransform().TransformPosition(Ship->Base), static_cast<float>(Ship->Beam), 400.0f, -1, true);
		}
	}
	else if (Effects)
	{
		// Settling, going down, rolling over: the water boiling white round her (the effects kit's churn: a low boil,
		// no flash; MoveShip keeps it going along her waterline), and a little smoke.
		Effects->ShellOfSize(GetActorTransform().TransformPosition(Ship->Base + Along * (Shown.FRandRange(-0.3f, 0.3f) * Ship->Long * U)), TEXT("churn"), TEXT("water"));
		if (Shown.FRand() < 0.5f)
		{
			Effects->Burn(GetActorTransform().TransformPosition(Ship->Base + FVector(0.0, 0.0, Ship->High * 0.25 * U)), static_cast<float>(Ship->Beam * 0.6), 600.0f, -1, true);
		}
	}
	// Her oil burning on the water where she lies: a fat black column off the slick, one fire a ship, its reach 0.6 of
	// her length, until the scene ends. A ship ablaze from end to end takes the same black column, its slick round her
	// and the fires on her deck at its foot (the art director on build 1734: Akagi's greyer deck column read pale
	// grey-brown, some 97 luma, where a carrier burning end to end sends up Pearl Harbor's black).
	// A submarine going down leaves none. At most MostOilFires columns (the effects kit, 05:25Z: at 24 the shell bursts
	// get no room at all): past that a battleship's or a carrier's puts out a destroyer's, and a destroyer's is not lit.
	if (Ship->Given.Class != TEXT("ss") && !OilLit.Contains(Id))
	{
		const int32 Rank = StageParts::OilRank(Ship->Given.Class);
		bool bLight = OilLit.Num() < MostOilFires;
		if (!bLight)
		{
			int32 Least = -1;
			for (const TPair<int32, int32>& Lit : OilLit)
			{
				if (Lit.Value < Rank && (Least < 0 || Lit.Value < OilLit.FindRef(Least)))
				{
					Least = Lit.Key;
				}
			}
			if (Least >= 0)
			{
				StageParts::OilStop(Effects, 238000 + Least);
				OilLit.Remove(Least);
				bLight = true;
			}
		}
		if (bLight)
		{
			// A ship lost to fire stands under her own column, leaning downwind and paling as it climbs (the effects kit's
			// "ablaze" row, 01f2dfc7; the art director on Midway 06: a soft black ball, not a column), grown as a fire
			// burning since her first hit and three minutes at least (Akagi burned for hours). One that goes down leaves
			// her burning oil, as before. A kit without the ablaze row draws "sunk".
			const bool bAblaze = How == TEXT("burn");
			const float Since = bAblaze ? static_cast<float>(FMath::Max(180.0, Ship->FirstHitAt >= 0.0 ? Now - Ship->FirstHitAt : 0.0)) : 0.0f;
			if (StageParts::OilFire(Effects, 238000 + Id, GetActorTransform().TransformPosition(Ship->Base), static_cast<float>(Ship->Long * 0.6), bAblaze ? TEXT("ablaze") : TEXT("sunk"), Since))
			{
				OilLit.Add(Id, Rank);
			}
		}
	}
	MoveShip(*Ship, Now);
}

void AAirStage::MoveShip(FShip& Ship, double Now)
{
	if (Ship.bGone || !Ship.Hull)
	{
		return;
	}
	if (Ship.How.IsEmpty())
	{
		// Afloat: her looks' list, trim and settle (a torpedo's flooding, a bow stove in), eased in, counter-flooded back.
		double List = 0.0, Trim = 0.0, Settle = 0.0;
		if (Looks.Pose(Ship.Given.Id, Now, List, Trim, Settle))
		{
			Settle = Ship.bDocked ? FMath::Min(Settle, 0.5) : Settle;
			Ship.Hull->SetRelativeLocationAndRotation(Ship.Base - FVector(0.0, 0.0, Settle * UnitsPerMetre), FRotator(Trim, Ship.Yaw, List));
		}
		return;
	}
	// (her end's pace: her look may make it quicker or slower than the stage's own)
	const double T = FMath::Max(0.0, Now - Ship.SunkAt) / FMath::Max(Ship.Pace, 0.25);
	// (each ship lists to a side of her own: the side she was flooding on, else her look's, else by her id)
	const double Side = Ship.Side != 0.0 ? Ship.Side : (Ship.Given.Id % 2) ? 1.0 : -1.0;
	// (and goes on from the list, trim and settle she had when her end came)
	double WasList = 0.0, WasTrim = 0.0, WasSettle = 0.0;
	Looks.Pose(Ship.Given.Id, Ship.SunkAt, WasList, WasTrim, WasSettle);
	// How deep she settles in harbour: decks awash on the bottom, as at Battleship Row (about 12 m of water).
	const double Settle = FMath::Min(Ship.High * 0.3, 11.0);
	const double Under = Ship.High + 40.0;
	double Depth = 0.0, Pitch = 0.0, Roll = 0.0, Break = 0.0;
	if (Ship.How == TEXT("settle"))
	{
		Depth = Settle * StageParts::Ease(T / 45.0);
		Roll = (Ship.Roll >= 0.0 ? FMath::Min(Ship.Roll, 25.0) : 7.0) * Side * StageParts::Ease(T / 45.0);
		Pitch = 1.5 * StageParts::Ease(T / 45.0);
	}
	else if (Ship.How == TEXT("sink"))
	{
		// (bow or stern first: her look's, else by the ship; on an even keel, all but level)
		const double Down = Ship.End == TEXT("bow") ? 1.0 : Ship.End == TEXT("stern") ? -1.0 : Ship.End == TEXT("even") ? 0.15 * Side : Side;
		Pitch = 14.0 * Down * StageParts::Ease(T / 35.0);
		Roll = 5.0 * Side * StageParts::Ease(T / 35.0);
		Depth = Under * StageParts::EaseIn(T / 70.0);
	}
	else if (Ship.How == TEXT("capsize"))
	{
		Roll = (Ship.bHarbour ? 150.0 : 165.0) * Side * StageParts::Ease(T / 28.0);
		Depth = Ship.Beam * 0.25 * StageParts::Ease(T / 28.0);
		if (!Ship.bHarbour)
		{
			Depth += Under * StageParts::EaseIn((T - 28.0) / 60.0);
		}
	}
	else if (Ship.How == TEXT("explode"))
	{
		// (the forward magazine: she goes down by the bow; the after one, by the stern)
		Pitch = (Ship.bHarbour ? 5.0 : 20.0) * (Ship.End == TEXT("aft") ? -1.0 : 1.0) * StageParts::Ease(T / (Ship.bHarbour ? 14.0 : 20.0));
		Depth = Ship.bHarbour ? Settle * 1.1 * StageParts::Ease(T / 14.0) : Under * StageParts::EaseIn(T / 30.0);
	}
	else if (Ship.How == TEXT("burn"))
	{
		Roll = 4.0 * Side * StageParts::Ease(T / 80.0);
		Depth = (Ship.bHarbour ? 2.5 : 1.5) * StageParts::Ease(T / 200.0);
	}
	else if (Ship.How == TEXT("break"))
	{
		Break = (Ship.bHarbour ? 12.0 : 28.0) * StageParts::Ease(T / 25.0);
		Depth = Ship.bHarbour ? 6.0 * StageParts::Ease(T / 20.0) : (Ship.High + 30.0) * StageParts::EaseIn(T / 40.0);
	}
	Roll += WasList;
	Pitch += WasTrim;
	Depth += WasSettle;
	if (Ship.bDocked)
	{
		// (on a dock's floor she goes no lower: Cassin and Downes burned out where they lay, Cassin rolled onto Downes)
		Depth = FMath::Min(Depth, 0.5);
	}
	const FVector At = Ship.Base - FVector(0.0, 0.0, Depth * UnitsPerMetre);
	if (Ship.Fore)
	{
		// The stern's half lifts its stern (+X up is a nose-up pitch), the bow's half its bow: a V, sinking at the break.
		Ship.Hull->SetRelativeLocationAndRotation(At, FRotator(Break, Ship.Yaw, Roll));
		Ship.Fore->SetRelativeLocationAndRotation(At, FRotator(-Break, Ship.Yaw, Roll));
	}
	else
	{
		// (a positive pitch lifts the model's +X, her stern: the bow goes down)
		Ship.Hull->SetRelativeLocationAndRotation(At, FRotator(Pitch, Ship.Yaw, Roll));
	}
	// The sea boiling round her while she settles, goes down, rolls over or breaks: the effects kit's churn, one to every
	// 25 to 30 m of her waterline, every 4 to 6 s (the effects kit, 22:59Z), for her first minute or so, off both sides.
	// (not on a dock's floor, and not for a ship burning on the water; a clock run back starts it again)
	if (Effects && Ship.How != TEXT("burn") && !Ship.bDocked && T < (Ship.bHarbour ? 50.0 : 80.0) && Depth < Ship.High
		&& (Now >= Ship.NextChurn || Now < Ship.NextChurn - 10.0))
	{
		Ship.NextChurn = Now + Shown.FRandRange(4.0f, 6.0f);
		const FRotator Turned(0.0, Ship.Yaw, 0.0);
		const FVector Along = Turned.RotateVector(FVector::ForwardVector), Abeam = Turned.RotateVector(FVector::RightVector);
		const int32 Churns = FMath::Clamp(FMath::RoundToInt(Ship.Long / Shown.FRandRange(25.0f, 30.0f)), 1, 7);
		for (int32 K = 0; K < Churns; ++K)
		{
			const double Share = (K + Shown.FRandRange(0.2f, 0.8f)) / Churns - 0.5;
			const double Off = (Ship.Beam * 0.5 + Shown.FRandRange(0.5f, 3.0f)) * (K % 2 ? 1.0 : -1.0);
			Effects->ShellOfSize(GetActorTransform().TransformPosition(At + (Along * (Share * Ship.Long) + Abeam * Off) * UnitsPerMetre), TEXT("churn"), TEXT("water"));
		}
	}
	if (Depth >= Ship.High + 25.0)
	{
		Ship.bGone = true;
		Ship.Hull->SetVisibility(false);
		if (Ship.Fore)
		{
			Ship.Fore->SetVisibility(false);
		}
	}
}

int32 AAirStage::BombOnOil(const FVector& WorldAt, TArray<TPair<FVector, float>>& OutBalls)
{
	if (!bRaised || !Effects)
	{
		return 0;
	}
	const double U = UnitsPerMetre;
	int32 Up = 0;
	for (int32 K = 0; K < Tanks.Num(); ++K)
	{
		const TArray<double>& Tank = Tanks[K];
		if (Tank.Num() < 5 || TanksAlight.Contains(K))
		{
			continue;
		}
		// (its roof's middle; the burst within its wall or 6 m of it: a 250 kg bomb's blast and splinters open a tank's
		// thin steel at that, and its oil goes up)
		const FVector Roof = StageToWorld(FVector(Tank[0], Tank[1], Tank[2] + Tank[4]));
		if (FVector::Dist2D(Roof, WorldAt) > (Tank[3] + 6.0) * U)
		{
			continue;
		}
		TanksAlight.Add(K);
		const float Across = static_cast<float>(FMath::Clamp(Tank[3] * 2.0, 15.0, 25.0));
		Effects->FuelBall(Roof, Across);
		Effects->Burn(Roof, static_cast<float>(Tank[3] * 2.0), 1800.0f);
		OutBalls.Add(TPair<FVector, float>(Roof, Across));
		++Up;
		UE_LOG(LogTheater, Display, TEXT("Air battle: an oil tank goes up (%d of %d alight), a fuel ball %.0f m across, %.0f m from the bomb."),
			TanksAlight.Num(), Tanks.Num(), Across, FVector::Dist2D(Roof, WorldAt) / U);
	}
	return Up;
}

void AAirStage::Advance(double Now)
{
	Clock = Now;
	// The steps of each hit's look now due, played where she lies now.
	Looks.Advance(Now, Effects, [this](int32 Id, FShipLie& Lie)
	{
		const FShip* Ship = Ships.Find(Id);
		if (!Ship || Ship->bGone)
		{
			return false;
		}
		Lie.World = GetActorTransform();
		// (her waterline where she lies now: settled, or going down, a late hit's burst stands on her, not above her)
		Lie.Base = Ship->Hull ? FVector(Ship->Base.X, Ship->Base.Y, Ship->Hull->GetRelativeLocation().Z) : Ship->Base;
		Lie.SeaZ = Ship->Base.Z;
		Lie.Yaw = Ship->Yaw;
		Lie.Long = Ship->Long;
		Lie.Beam = Ship->Beam;
		Lie.High = Ship->High;
		Lie.UnitsPerMetre = UnitsPerMetre;
		return true;
	});
	for (TPair<int32, FShip>& Pair : Ships)
	{
		MoveShip(Pair.Value, Now);
	}
}

// ---------------------------------------------------------------------------------------------------------------
// The stage test: "-TheaterStage=oahu" (or "midway", "midway-fleet"), "-TheaterStageShots=<folder>".

bool AAirStage::WantsTest(FString& OutKey)
{
	return FParse::Value(FCommandLine::Get(), TEXT("TheaterStage="), OutKey) && !OutKey.IsEmpty();
}

void AAirStage::BeginTest(const FString& Key)
{
	// (begun two seconds in, once the table and the hour's light are up, so that they are put away after)
	PendingTest = Key;
	SetActorTickEnabled(true);
}

void AAirStage::StartTest(const FString& Key)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const bool bFleet = Key == TEXT("midway-fleet");
	const FString Stage = bFleet ? FString(TEXT("midway")) : Key;
	FVector2D Aim = FVector2D::ZeroVector;
	if (!Raise(Stage, Aim, 0.0, FVector2D::ZeroVector, 100.0, nullptr))
	{
		QuitTest(FString::Printf(TEXT("the stage %s could not be raised (is Content/Data/Stages/%s.json staged with the build?)"), *Stage, *Stage));
		return;
	}
	if (bFleet && TestBook.IsValid())
	{
		// (off Midway the strike is on the Kido Butai at sea: the board is centred on its box)
		const TArray<double> At = StageParts::Numbers(TestBook->TryGetField(TEXT("aim")));
		if (At.Num() == 2)
		{
			StageAim = FVector2D(At[0], At[1]);
			for (TObjectPtr<UTheaterStillMesh>& Mesh : StillMeshes)
			{
				Mesh->DestroyComponent();
			}
			StillMeshes.Reset();
			BuildSea();
			BuildGround();
			BuildWorks();
		}
	}
	StageKey = Key;
	OwnEffects = MakeUnique<FBattleEffects>();
	if (OwnEffects->Begin(Root, UnitsPerMetre, [this](const FVector& Point) { return FVector(Point.X, Point.Y, GroundZ(Point)); }))
	{
		Effects = OwnEffects.Get();
		Effects->SetSun(TowardSun());
		Effects->SetWind(FVector(-2.0, 3.0, 0.0)); // (the trade wind, from the north-east)
	}
	// The record's ships, numbered from 1 in the file's order (Pearl Harbor's berths; the Kido Butai's box).
	if (Key != TEXT("midway") && TestBook.IsValid())
	{
		const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
		if (TestBook->TryGetArrayField(TEXT("ships"), List))
		{
			TArray<FStageShip> Given;
			for (int32 K = 0; K < List->Num(); ++K)
			{
				const TSharedPtr<FJsonObject>* Item = nullptr;
				if (!(*List)[K]->TryGetObject(Item))
				{
					continue;
				}
				FStageShip& Ship = Given.AddDefaulted_GetRef();
				Ship.Id = K + 1;
				(*Item)->TryGetStringField(TEXT("cls"), Ship.Class);
				(*Item)->TryGetStringField(TEXT("nat"), Ship.Nation);
				(*Item)->TryGetStringField(TEXT("name"), Ship.Name);
				(*Item)->TryGetBoolField(TEXT("dock"), Ship.bDock);
				const TArray<double> At = StageParts::Numbers((*Item)->TryGetField(TEXT("at")));
				double Face = 0.0;
				(*Item)->TryGetNumberField(TEXT("face"), Face);
				if (At.Num() == 2)
				{
					Ship.At = StageToBoard(FVector2D(At[0], At[1]));
				}
				Ship.Face = FMath::DegreesToRadians(Face) - StageTurn;
			}
			SetShips(Given);
		}
	}
	// The table and its armies are put out of sight; the light and the air are the hour's at the stage.
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->IsA<ACampaignMapActor>() || It->IsA<AArmyPiece>())
		{
			It->SetActorHiddenInGame(true);
		}
		if (ACampaignEnvironment* Environment = Cast<ACampaignEnvironment>(*It))
		{
			Environment->SetActorTickEnabled(false);
			double Density = 0.0006, Falloff = 0.005;
			FString Asked;
			TArray<FString> Bits;
			// ("-TheaterStageFog=density,falloff" tries other air)
			if (FParse::Value(FCommandLine::Get(), TEXT("TheaterStageFog="), Asked, false) && Asked.ParseIntoArray(Bits, TEXT(",")) == 2)
			{
				Density = FCString::Atod(*Bits[0]);
				Falloff = FCString::Atod(*Bits[1]);
			}
			TArray<UExponentialHeightFogComponent*> Fogs;
			Environment->GetComponents(Fogs);
			for (UExponentialHeightFogComponent* Fog : Fogs)
			{
				Fog->SetStartDistance(0.0f);
				Fog->SetFogDensity(static_cast<float>(Density));
				Fog->SetFogHeightFalloff(static_cast<float>(Falloff));
				Fog->SetFogMaxOpacity(0.85f);
				Fog->SetFogInscatteringColor(FLinearColor(0.55f, 0.66f, 0.78f));
			}
			TArray<UDirectionalLightComponent*> Lights;
			Environment->GetComponents(Lights);
			UDirectionalLightComponent* Brightest = nullptr;
			for (UDirectionalLightComponent* Light : Lights)
			{
				if (!Brightest || Light->Intensity > Brightest->Intensity)
				{
					Brightest = Light;
				}
			}
			if (Brightest)
			{
				Brightest->SetWorldRotation((-TowardSun()).Rotation());
			}
		}
	}
	TestCamera = World->SpawnActor<ACameraActor>();
	if (TestCamera)
	{
		// (the whole window, whatever its shape: the first pictures had black bars above and below)
		TestCamera->GetCameraComponent()->bConstrainAspectRatio = false;
	}
	if (APlayerController* Controller = World->GetFirstPlayerController())
	{
		if (TestCamera)
		{
			Controller->SetViewTarget(TestCamera);
		}
		if (Controller->GetHUD())
		{
			Controller->GetHUD()->bShowHUD = false;
		}
	}
	FParse::Value(FCommandLine::Get(), TEXT("TheaterStageShots="), TestFolder);
	if (!TestCamera)
	{
		QuitTest(TEXT("the test camera did not spawn"));
		return;
	}
	if (!TestFolder.IsEmpty())
	{
		IFileManager::Get().MakeDirectory(*TestFolder, true);
	}
	TestPlan();
	bTest = true;
	TestStart = World->GetTimeSeconds();
	SetActorTickEnabled(true);
	const FString Where = TestFolder.IsEmpty() ? FString(TEXT(" (no folder: watch it)")) : TEXT(" into ") + TestFolder;
	UE_LOG(LogTheater, Display, TEXT("Stage test: %s, %d ships, %d pictures%s."), *Key, Ships.Num(), TestShots.Num(), *Where);
}

void AAirStage::TestPlan()
{
	TestShots.Reset();
	TestBeats.Reset();
	auto IdOf = [this](const TCHAR* Name) -> int32
	{
		for (const TPair<int32, FShip>& Pair : Ships)
		{
			if (Pair.Value.Given.Name == Name)
			{
				return Pair.Key;
			}
		}
		return 0;
	};
	auto Shot = [this](const TCHAR* Name, double At, const FVector& Eye, const FVector& Look, double Lens)
	{
		TestShots.Add({ Name, At, Eye, Look, Lens });
	};
	auto Beat = [this](double At, int32 Ship, const TCHAR* What, const TCHAR* How, bool bBig = false)
	{
		if (Ship > 0)
		{
			TestBeats.Add({ At, Ship, What, How, bBig });
		}
	};
	if (FParse::Param(FCommandLine::Get(), TEXT("TheaterStageReel")) && Looks.IsReady())
	{
		// The looks reel: each look a ship can show when she survives a hit, one after another on a ship the test keeps
		// afloat, two pictures each (the burst, then what follows), so the art director sees every one (the pre-check,
		// 20 November 15:19Z: in the battles themselves nearly every recorded hit comes with the ship's end).
		// (Nevada, a battleship in harbour; Hiryu, a carrier at sea, for the deck park and the hangar)
		int32 Order = 0;
		if (const FShip* Ship = Ships.Find(IdOf(StageKey == TEXT("oahu") ? TEXT("Nevada") : TEXT("Hiryu"))))
		{
			const FVector2D Stage = BoardToStage(Ship->Given.At);
			const double Reach = FMath::Max(Ship->Long * 1.6, 160.0);
			const FVector Her(Stage.X, Stage.Y, 6.0), Eye(Stage.X - Reach * 0.8, Stage.Y - Reach * 0.6, FMath::Max(Ship->High * 1.4, 24.0));
			for (const FString& Look : Looks.ReelOf(Ship->Given.Class, Ship->bHarbour))
			{
				const double At = 4.0 + Order * 44.0;
				TestBeats.Add({ At, Ship->Given.Id, TEXT("look"), Look, false });
				const FString Key = Look.Left(Look.Find(TEXT("|"))).Replace(TEXT("ship."), TEXT(""));
				TestShots.Add({ FString::Printf(TEXT("reel-%02d-%s-a"), Order + 1, *Key), At + 1.5, Eye, Her, 40.0 });
				TestShots.Add({ FString::Printf(TEXT("reel-%02d-%s-b"), Order + 1, *Key), At + 40.0, Eye, Her, 40.0 });
				++Order;
			}
		}
		UE_LOG(LogTheater, Display, TEXT("Stage test: the looks reel, %d looks."), Order);
		return;
	}
	if (StageKey == TEXT("oahu"))
	{
		// 7 December 1941, Battleship Row (the origin is berth F-6, the row runs south-west to north-east).
		Beat(4.0, IdOf(TEXT("West Virginia")), TEXT("hit"), TEXT("torpedo"));
		Beat(4.5, IdOf(TEXT("Oklahoma")), TEXT("hit"), TEXT("torpedo"));
		Beat(5.0, IdOf(TEXT("California")), TEXT("hit"), TEXT("torpedo"));
		Beat(6.0, IdOf(TEXT("Oklahoma")), TEXT("sink"), TEXT("capsize"));
		Beat(9.0, IdOf(TEXT("Arizona")), TEXT("hit"), TEXT("level"));
		Beat(9.3, IdOf(TEXT("Arizona")), TEXT("sink"), TEXT("explode"), true);
		Beat(12.0, IdOf(TEXT("West Virginia")), TEXT("sink"), TEXT("settle"));
		Beat(14.0, IdOf(TEXT("California")), TEXT("sink"), TEXT("settle"));
		Beat(16.0, IdOf(TEXT("Nevada")), TEXT("hit"), TEXT("dive"));
		Beat(18.0, IdOf(TEXT("Shaw")), TEXT("sink"), TEXT("explode"));
		Beat(20.0, IdOf(TEXT("Cassin")), TEXT("sink"), TEXT("capsize"));
		Beat(20.5, IdOf(TEXT("Downes")), TEXT("sink"), TEXT("burn"));
		Shot(TEXT("01-approach"), 3.0, FVector(-9000, 26000, 3200), FVector(-1000, 0, 0), 45);
		Shot(TEXT("02-torpedo-run"), 5.2, FVector(1100, -900, 18), FVector(0, 0, 8), 40);
		Shot(TEXT("03-arizona"), 9.9, FVector(700, -350, 60), FVector(181, 115, 30), 45);
		Shot(TEXT("04-row-from-ford-island"), 26.0, FVector(-230, 260, 14), FVector(300, -120, 12), 50);
		Shot(TEXT("05-dive-view"), 30.0, FVector(-600, -2200, 3300), FVector(0, 0, 0), 35);
		Shot(TEXT("06-aftermath"), 70.0, FVector(1500, -1500, 120), FVector(0, 0, 10), 40);
	}
	else if (StageKey == TEXT("midway-fleet"))
	{
		// 4 June 1942, the Kido Butai in its box (stage metres: the box is round the aim).
		const FVector A(StageAim.X, StageAim.Y, 0.0);
		Beat(4.0, IdOf(TEXT("Kaga")), TEXT("hit"), TEXT("dive"));
		Beat(4.3, IdOf(TEXT("Kaga")), TEXT("hit"), TEXT("dive"));
		Beat(4.6, IdOf(TEXT("Kaga")), TEXT("hit"), TEXT("dive"));
		Beat(5.0, IdOf(TEXT("Akagi")), TEXT("hit"), TEXT("dive"));
		Beat(5.2, IdOf(TEXT("Soryu")), TEXT("hit"), TEXT("dive"));
		Beat(5.5, IdOf(TEXT("Soryu")), TEXT("hit"), TEXT("dive"));
		Beat(6.0, IdOf(TEXT("Kaga")), TEXT("sink"), TEXT("burn"));
		Beat(7.0, IdOf(TEXT("Soryu")), TEXT("sink"), TEXT("burn"));
		Beat(8.0, IdOf(TEXT("Akagi")), TEXT("sink"), TEXT("burn"));
		Beat(12.0, IdOf(TEXT("Hiryu")), TEXT("hit"), TEXT("dive"));
		Beat(12.5, IdOf(TEXT("Hiryu")), TEXT("sink"), TEXT("explode"), true);
		// (a destroyer of the screen breaking in two, to see it)
		const int32 Screen = Ships.Num() > 0 ? Ships.Num() : 0;
		Beat(14.0, Screen, TEXT("sink"), TEXT("break"));
		Shot(TEXT("01-dive-view"), 3.0, A + FVector(-600, -900, 4200), A, 35);
		Shot(TEXT("02-hits"), 4.8, A + FVector(-1800, -2400, 300), A + FVector(-1250, 1250, 10), 40);
		if (const FShip* Last = Ships.Find(Screen))
		{
			const FVector2D Stage = BoardToStage(Last->Given.At);
			Shot(TEXT("03-break"), 26.0, FVector(Stage.X + 300, Stage.Y - 400, 30), FVector(Stage.X, Stage.Y, 0), 45);
		}
		Shot(TEXT("04-carriers-burning"), 30.0, A + FVector(2500, -3500, 150), A, 40);
		Shot(TEXT("05-far"), 60.0, A + FVector(-9000, -6000, 1500), A, 40);
	}
	else
	{
		// Midway's atoll: the oil tanks on Sand Island and the hangars on Eastern Island bombed and burning.
		Shot(TEXT("01-atoll-approach"), 3.0, FVector(-9000, 8000, 3000), FVector(0, 0, 0), 45);
		Shot(TEXT("02-sand-island-tanks"), 12.0, FVector(-900, -1200, 80), FVector(-1200, -10, 10), 45);
		Shot(TEXT("03-eastern-island"), 14.0, FVector(1200, -600, 60), FVector(2256, 498, 5), 45);
	}
}

void AAirStage::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!PendingTest.IsEmpty() && GetWorld() && GetWorld()->GetTimeSeconds() > 2.0)
	{
		const FString Key = PendingTest;
		PendingTest.Empty();
		StartTest(Key);
	}
	if (bTest)
	{
		TestTick();
	}
}

void AAirStage::QuitTest(const FString& Why)
{
	// A picture run that cannot go on says why and stops, rather than hang the builder's run.
	UE_LOG(LogTheater, Error, TEXT("Stage test: %s; stopping."), *Why);
	// (only the command line's -TheaterStage starts a test, so there is no game here to keep running)
	bTest = false;
	SetActorTickEnabled(false);
	UKismetSystemLibrary::QuitGame(this, nullptr, EQuitPreference::Quit, false);
	FPlatformMisc::RequestExit(false);
}

void AAirStage::TestTick()
{
	UWorld* World = GetWorld();
	if (!World || !TestCamera)
	{
		return;
	}
	const double Now = World->GetTimeSeconds() - TestStart;
	while (NextBeat < TestBeats.Num() && TestBeats[NextBeat].At <= Now)
	{
		const FTestBeat& Beat = TestBeats[NextBeat++];
		if (Beat.What == TEXT("hit"))
		{
			Hit(Beat.Ship, Beat.How, Now);
		}
		else if (Beat.What == TEXT("look"))
		{
			if (const FShip* Ship = Ships.Find(Beat.Ship))
			{
				// Each look alone: the kit cleared of the last look's smoke, fire and steam first (the art director, 20
				// November 17:54Z: from the fourth hit on, each look sat inside the smoke of the ones before it).
				if (Effects)
				{
					Effects->Clear();
					Looks.Cleared();
				}
				Looks.Reel(Beat.Ship, Ship->Given.Class, Beat.How, Ship->bHarbour, Now);
				// And the camera on the look: its first picture close on the hit, from the side it is on and a little
				// toward her nearer end; its second further off on that quarter and lower, where a list, a trim or a lower
				// freeboard shows. (over land, it stands at least 12 m above the ground)
				double Along = 0.0, Up = 0.0, Side = 1.0;
				if (Looks.ReelAim(Beat.Ship, Ship->High, Along, Up, Side))
				{
					const double U = UnitsPerMetre;
					const FRotator Turned(0.0, Ship->Yaw, 0.0);
					const FVector Length = Turned.RotateVector(FVector::ForwardVector), Abeam = Turned.RotateVector(FVector::RightVector);
					const double End = Along < 0.0 ? -1.0 : 1.0; // (her bow's end, minus, or her stern's)
					const double Near = FMath::Max(Ship->Long * 0.55, 70.0), Far = FMath::Max(Ship->Long * 1.1, 130.0);
					const FVector Hit = Ship->Base + Length * (Along * Ship->Long * U) + FVector(0.0, 0.0, Up * U);
					const FVector Middle = Ship->Base + Length * (Along * 0.5 * Ship->Long * U) + FVector(0.0, 0.0, FMath::Min(Ship->High * 0.3, 9.0) * U);
					auto ToStage = [this](const FVector& Local)
					{
						const FVector2D Flat = BoardToStage(FVector2D(Local.Y, Local.X) / UnitsPerMetre);
						return FVector(Flat.X, Flat.Y, Local.Z / UnitsPerMetre);
					};
					auto Above = [this](FVector Stage)
					{
						Stage.Z = FMath::Max(Stage.Z, StageHeight(Stage.X, Stage.Y) + 12.0);
						return Stage;
					};
					const FVector EyeA = Above(ToStage(Hit + (Abeam * (Side * Near) + Length * (End * Near * 0.45)) * U + FVector(0.0, 0.0, FMath::Max(Ship->High * 0.8, 16.0) * U)));
					const FVector EyeB = Above(ToStage(Ship->Base + (Abeam * (Side * Far * 0.7) + Length * (End * Far * 0.75)) * U + FVector(0.0, 0.0, FMath::Max(Ship->High * 0.5, 12.0) * U)));
					for (int32 K = NextShot; K < TestShots.Num() && K < NextShot + 3; ++K)
					{
						FTestShot& Shot = TestShots[K];
						if (Shot.At >= Now && Shot.At <= Now + 43.0 && Shot.Name.StartsWith(TEXT("reel-")))
						{
							const bool bFirst = Shot.Name.EndsWith(TEXT("-a"));
							Shot.Eye = bFirst ? EyeA : EyeB;
							Shot.Look = ToStage(bFirst ? Hit : Middle);
							Shot.Lens = bFirst ? 45.0 : 50.0;
						}
					}
				}
			}
		}
		else
		{
			Sink(Beat.Ship, Beat.How, Beat.bBig, Now);
		}
	}
	if (StageKey == TEXT("midway") && NextBeat == 0 && Now >= 4.0 && Effects)
	{
		// (the atoll has no ships in the test: the bombs fall on its tanks and hangars)
		NextBeat = 1;
		for (const TArray<double>& Tank : Tanks)
		{
			const FVector Spot = StageToWorld(FVector(Tank[0], Tank[1], Tank[2] + Tank[4]));
			Effects->ShellOfSize(Spot, TEXT("bomb250"), TEXT("sand"));
			// (the fuel going up off its roof, as BombOnOil has it in the battle)
			Effects->FuelBall(Spot, static_cast<float>(FMath::Clamp(Tank[3] * 2.0, 15.0, 25.0)));
			Effects->Burn(Spot, static_cast<float>(Tank[3] * 2.0), 1800.0f);
		}
		for (int32 K = 0; K < Hangars.Num(); K += 2)
		{
			const FVector Spot = StageToWorld(FVector(Hangars[K][0], Hangars[K][1], Hangars[K][2]));
			Effects->ShellOfSize(Spot, TEXT("bomb250"), TEXT("sand"));
			Effects->Burn(Spot, 20.0f, 900.0f);
		}
	}
	Advance(Now);
	// The camera stands at the next picture's place from the moment the last one was taken.
	if (NextShot < TestShots.Num())
	{
		const FTestShot& Shot = TestShots[NextShot];
		const FVector Eye = StageToWorld(Shot.Eye), Look = StageToWorld(Shot.Look);
		TestCamera->SetActorLocationAndRotation(Eye, (Look - Eye).Rotation());
		TestCamera->GetCameraComponent()->SetFieldOfView(static_cast<float>(Shot.Lens));
		if (Now >= Shot.At && ShotTakenAt < 0.0)
		{
			if (!TestFolder.IsEmpty())
			{
				FScreenshotRequest::RequestScreenshot(FPaths::Combine(TestFolder, StageKey + TEXT("-") + Shot.Name + TEXT(".png")), false, false);
			}
			UE_LOG(LogTheater, Display, TEXT("Stage test: %s at %.1f s."), *Shot.Name, Now);
			ShotTakenAt = Now;
		}
		else if (ShotTakenAt >= 0.0 && Now >= ShotTakenAt + 0.5)
		{
			// (half a second for the picture to be written before the camera moves on)
			ShotTakenAt = -1.0;
			++NextShot;
		}
	}
	else if (!TestFolder.IsEmpty())
	{
		UE_LOG(LogTheater, Display, TEXT("Stage test: done."));
		bTest = false;
		UKismetSystemLibrary::QuitGame(this, nullptr, EQuitPreference::Quit, false);
		FPlatformMisc::RequestExit(false);
	}
	if (Effects)
	{
		int32 Wide = 1600, High = 1000;
		if (APlayerController* Controller = World->GetFirstPlayerController())
		{
			Controller->GetViewportSize(Wide, High);
		}
		Effects->Tick(Now, TestCamera->GetActorLocation(), TestCamera->GetActorRotation(), TestCamera->GetCameraComponent()->FieldOfView, Wide);
	}
}
