#include "Battle/SeaWeather.h"

#include "Battle/AirSky.h"
#include "Campaign/CampaignTextures.h"
#include "Campaign/TheaterStillMesh.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Async/Async.h"
#include "Engine/Texture2D.h"
#include "HAL/PlatformTime.h"
#include "ProceduralMeshComponent.h"
#include "TextureResource.h"
#include "TheaterCommand.h"
#include "UObject/UObjectGlobals.h"

#include <algorithm>
#include <cmath>

namespace SeaWeatherParts
{
	/** The sky dome: 48 by 12 quads at 150 km (the plan's step 13(b)), its skirt a little under the horizon so the sea's
	 * far edge never shows a gap under it. */
	static constexpr int32 DomeAround = 48, DomeRows = 12;
	static constexpr double DomeFar = 150000.0, DomeSkirt = -3.0;
	/** The clouds' own ink, as the air battle has it (AAirBattle's CloudBright): the painter paints the shaded sides and
	 * the bases that much darker, and the material lifts the whole by it, so only the sunlit tops rise. */
	static constexpr double CloudInk = 1.3;
	/** How often the dome is painted again while nothing cuts (s), and how far the eye may go first (m): a cut always
	 * paints it again. */
	static constexpr double PaintEvery = 8.0, PaintMoved = 1200.0;
	/** The ticks a paint asked at a cut is given before the frame waits for it (ASeaBattle's SettleTicks is 10). */
	static constexpr int32 PaintTicks = 8;
	/** The rain's grey (the air battle's rain shafts). */
	static const FLinearColor RainInk(0.42f, 0.44f, 0.47f, 1.0f);
	/** The haze band over the sea's far edge, as the air battle has it: the sea's square each way (m), and the eye's
	 * height it fades in over. */
	static constexpr double SeaReach = 200000.0, BandFrom = 8.0, BandFull = 60.0;
	/** The haze's own ink (AirParts::AirHazeInk, written out: the sea scene does not take the air battle's parts). */
	static const FLinearColor HazeInk(0.80f, 0.84f, 0.88f, 1.0f);

	static UMaterialInterface* Load(const TCHAR* Path)
	{
		return LoadObject<UMaterialInterface>(nullptr, Path);
	}

	/** A picture the chip reads straight (no smaller copies, no sRGB), its columns coming round and its rows held: the
	 * sky's own. */
	static UTexture2D* MakePicture(int32 Wide, int32 High, const FName& Name)
	{
		UTexture2D* Made = UTexture2D::CreateTransient(Wide, High, PF_B8G8R8A8, Name);
		if (!Made)
		{
			return nullptr;
		}
		Made->SRGB = false;
		Made->CompressionSettings = TC_VectorDisplacementmap;
		Made->Filter = TF_Bilinear;
		Made->AddressX = TA_Wrap;
		Made->AddressY = TA_Clamp;
		Made->NeverStream = true;
		Made->UpdateResource();
		return Made;
	}

	/** The picture's pixels handed to the chip, which reads the copy when it comes to it. */
	static void Hand(UTexture2D* To, const TArray<FColor>& Pixels)
	{
		if (!To || !To->GetResource() || Pixels.Num() != To->GetSizeX() * To->GetSizeY())
		{
			return;
		}
		const int32 Wide = To->GetSizeX(), High = To->GetSizeY();
		FColor* Copy = static_cast<FColor*>(FMemory::Malloc(static_cast<SIZE_T>(Wide) * High * sizeof(FColor)));
		FMemory::Memcpy(Copy, Pixels.GetData(), static_cast<SIZE_T>(Wide) * High * sizeof(FColor));
		FUpdateTextureRegion2D* Region = new FUpdateTextureRegion2D(0, 0, 0, 0, Wide, High);
		To->UpdateTextureRegions(0, 1, Region, Wide * sizeof(FColor), sizeof(FColor), reinterpret_cast<uint8*>(Copy),
			[](uint8* Data, const FUpdateTextureRegion2D* Regions)
			{
				FMemory::Free(Data);
				delete Regions;
			});
	}
}

FSeaWeather::~FSeaWeather()
{
	// (nothing of the actor's is touched here: the paint on the pool thread writes only this one's own picture)
	if (Job.IsValid())
	{
		Job.Wait();
	}
}

FVector FSeaWeather::World(double East, double North, double Up) const
{
	// (ASeaBattle::World: the world's x is the board's north, its y the board's east)
	const double U = Room.UnitsPerMetre;
	return FVector(North * U, East * U, Up * U);
}

void FSeaWeather::Begin(const FRoom& InRoom, SeaLookSwitches::Look& InForce, const FString& RecordingText, uint64 InSeed,
	double MiddleX, double MiddleY, const FVector2D& WindBoard)
{
	Room = InRoom;
	Seed = InSeed;
	MiddleEast = MiddleX;
	MiddleNorth = MiddleY;
	Wind = WindBoard;
	Switches = SeaSky::Take(InForce);
	// (the day: the rules' field sky, else the stand-in where asked, else none; a review day overrides both)
	std::string Why;
	const SeaSky::Given FromRules = SeaSky::ReadRecording(std::string(TCHAR_TO_UTF8(*RecordingText)), &Why);
	Chosen = SeaSky::Choose(FromRules, Switches);
	if (!Switches.Any())
	{
		UE_LOG(LogTheater, Display, TEXT("Sea weather: nothing asked (\"-TheaterSeaClouds=dome\", \"-TheaterSeaCloudShade=1\", \"-TheaterSeaRain=1\"): the sky is as it was."));
		return;
	}
	if (!Chosen.bLaid)
	{
		UE_LOG(LogTheater, Display, TEXT("Sea weather: %s (\"-TheaterSeaSkyStandIn=1\" lays air's own stand-in cumulus)."), *FString(UTF8_TO_TCHAR(Chosen.Why.c_str())));
		return;
	}
	if (Room.bNight)
	{
		UE_LOG(LogTheater, Display, TEXT("Sea weather: %s, but a night action keeps its own sky (the stars and the moon are the night dome's)."),
			*FString(UTF8_TO_TCHAR(Chosen.Why.c_str())));
		return;
	}
	// (the sun in the board's own axes: the world's x is the board's north)
	const FVector Sun = Room.Sun.GetSafeNormal();
	const SeaSky::Vec Board(Sun.Y, Sun.X, Sun.Z);
	const AirSky::Placing Where = SeaSky::PlacingFor(Seed, MiddleEast, MiddleNorth, Board, SeaSky::Vec(Wind.X, Wind.Y, 0.0));
	Sky = SeaSky::Lay(Chosen, Where);
	if (Sky.Clouds.empty())
	{
		UE_LOG(LogTheater, Display, TEXT("Sea weather: %s, and it laid no cloud (a clear day)."), *FString(UTF8_TO_TCHAR(Chosen.Why.c_str())));
		return;
	}
	bOn = true;
	UE_LOG(LogTheater, Display, TEXT("Sea weather: %s; %d clouds and %d puffs laid on a %.0f km square, base %.0f m, top %.0f m, cover %.2f, rain %.2f."),
		*FString(UTF8_TO_TCHAR(Chosen.Why.c_str())), static_cast<int32>(Sky.Clouds.size()), static_cast<int32>(Sky.Puffs.size()),
		AirSky::Period / 1000.0, Sky.Base, Sky.Top, Sky.Cover, Chosen.Rain);
	if (Switches.Draw != SeaSky::Clouds::Off)
	{
		LayDome();
		if (Chosen.Day.bDeck)
		{
			LayDeckSheet();
			LayHazeBand();
		}
	}
	if (Switches.bShade)
	{
		LayShade();
	}
	if (Switches.bRain)
	{
		LayRain();
	}
	SetLightEverywhere();
}

// ---- The dome -------------------------------------------------------------------------------------------------------

void FSeaWeather::LayDome()
{
	using namespace SeaWeatherParts;
	if (!Room.Owner || !Room.Root)
	{
		return;
	}
	UMaterialInterface* Ink = Load(TEXT("/Game/Campaign/Materials/M_SkyDome.M_SkyDome"));
	if (!Ink)
	{
		UE_LOG(LogTheater, Warning, TEXT("Sea weather: M_SkyDome is missing (run Tools/build_content.sh): no cloud dome."));
		return;
	}
	Picture.Reset(MakePicture(SeaSky::DomeW, SeaSky::DomeH, MakeUniqueObjectName(GetTransientPackage(), UTexture2D::StaticClass(), TEXT("SeaSkyDome"))));
	if (!Picture.IsValid())
	{
		return;
	}
	Pixels.SetNumZeroed(SeaSky::DomeW * SeaSky::DomeH);
	// A hemisphere of DomeAround columns and DomeRows rows at DomeFar, its lowest ring a little under the horizon. The
	// material reads the picture by the camera's own way, so where the quads fall does not matter: only that the dome
	// stands round the eye and behind everything else.
	UTheaterStillMesh* Mesh = UTheaterStillMesh::Make(Room.Owner, TEXT("SeaSkyDome"));
	if (!Mesh)
	{
		return;
	}
	Dome = Mesh;
	Mesh->bUseAsyncCooking = false;
	Mesh->SetupAttachment(Room.Root);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
	// (behind every other see-through thing of the weather: the haze band at -48, the far deck sheet at -45, the rain
	// nearer still; the night dome stands at 400 km and is sorted behind by depth)
	Mesh->SetTranslucentSortPriority(-49);
	Mesh->RegisterComponent();
	TArray<FVector> Points;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FColor> Colours;
	const double U = Room.UnitsPerMetre;
	for (int32 Row = 0; Row <= DomeRows; ++Row)
	{
		const double Up = (DomeSkirt + (90.0 - DomeSkirt) * Row / DomeRows) * UE_DOUBLE_PI / 180.0;
		for (int32 K = 0; K <= DomeAround; ++K)
		{
			const double Round = 2.0 * UE_DOUBLE_PI * K / DomeAround;
			const double Flat = FMath::Cos(Up);
			// (the board's bearing east of north: east is sin, north is cos)
			Points.Add(World(DomeFar * Flat * FMath::Sin(Round), DomeFar * Flat * FMath::Cos(Round), DomeFar * FMath::Sin(Up)));
			Normals.Add(FVector::UpVector);
			Colours.Add(FColor::White);
		}
	}
	const int32 Across = DomeAround + 1;
	for (int32 Row = 0; Row < DomeRows; ++Row)
	{
		for (int32 K = 0; K < DomeAround; ++K)
		{
			const int32 A = Row * Across + K, B = A + 1, C = (Row + 1) * Across + K + 1, D = (Row + 1) * Across + K;
			// (both ways round: the dome is seen from inside, whichever way the engine takes as its face)
			Triangles.Append({ A, B, C, A, C, D, A, C, B, A, D, C });
		}
	}
	Mesh->CreateMeshSection(0, Points, Triangles, Normals, TArray<FVector2D>(), Colours, TArray<FProcMeshTangent>(), false);
	DomeInk = UMaterialInstanceDynamic::Create(Ink, Room.Owner);
	if (UMaterialInstanceDynamic* Mid = DomeInk.Get())
	{
		Mid->SetTextureParameterValue(TEXT("Picture"), Picture.Get());
		Mid->SetScalarParameterValue(TEXT("Brightness"), static_cast<float>(CloudInk));
		Mid->SetScalarParameterValue(TEXT("DayLight"), static_cast<float>(Lit));
		Mid->SetScalarParameterValue(TEXT("LayerHeight"), static_cast<float>(0.5 * (Sky.Base + Sky.Top) * U));
		Mid->SetVectorParameterValue(TEXT("Drift"), FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
		Mesh->SetMaterial(0, Mid);
	}
	Brush = std::make_shared<AirSky::Painter>();
	// (the sheet of torn cloud shapes, as M_Cloud draws them in the air battle: made once, here on this thread, since a
	// sea scene opens before its first cut)
	AirSky::MakeFragments(Shapes);
	UE_LOG(LogTheater, Display, TEXT("Sea weather: a cloud dome of %d by %d quads at %.0f km, its picture %d by %d, painted again at each cut (%d puffs at most)."),
		DomeAround, DomeRows, DomeFar / 1000.0, SeaSky::DomeW, SeaSky::DomeH, SeaSky::MostPuffs);
}

void FSeaWeather::AskPaint(const FVector& EyeWorld, double BattleSeconds)
{
	if (!Brush || !Picture.IsValid() || Job.IsValid())
	{
		return;
	}
	const double U = Room.UnitsPerMetre;
	const SeaSky::Vec At(EyeWorld.Y / U, EyeWorld.X / U, EyeWorld.Z / U);
	const FVector Sun = Room.Sun.GetSafeNormal();
	const SeaSky::Vec Way(Sun.Y, Sun.X, Sun.Z);
	const SeaSky::Vec Drift = Sky.DriftAt(BattleSeconds);
	Painting = std::make_shared<SeaSky::Dome>();
	AskedEye = EyeWorld;
	AskedAt = BattleSeconds;
	SinceAsked = 0;
	bWaiting = true;
	SeaSky::DomeHow How;
	How.Sheet = &Shapes;
	How.Ink = SeaWeatherParts::CloudInk;
	// (the field, the sheet and the painter are this one's own and live as long as it does; the paint writes only into
	// the picture it was given)
	Job = Async(EAsyncExecution::ThreadPool, [this, At, Way, Drift, How, Into = Painting, With = Brush]()
	{
		const double Began = FPlatformTime::Seconds();
		SeaSky::PaintDome(Sky, At, Drift, Way, AirSky::Rgb(1.0f, 1.0f, 1.0f), *With, *Into, How);
		Into->Painted = FPlatformTime::Seconds() - Began;
	});
}

void FSeaWeather::TakePaint()
{
	if (!bWaiting || !Job.IsValid())
	{
		return;
	}
	if (!Job.IsReady())
	{
		// (in a picture run, a paint asked at a cut is given its ticks and then waited for, so the frame that is kept has
		// the right sky; in play it is never waited for: a full dome is some tenths of a second on one core, and a stall
		// at every cut would cost more than a sky a few frames late)
		if (!Room.bPictures || SinceAsked < SeaWeatherParts::PaintTicks)
		{
			++SinceAsked;
			return;
		}
		Job.Wait();
		++Waits;
	}
	Job.Reset();
	bWaiting = false;
	if (!Painting || Painting->Rgba.size() != static_cast<size_t>(SeaSky::DomeW) * SeaSky::DomeH * 4)
	{
		return;
	}
	const double Began = FPlatformTime::Seconds();
	Painted = Painting;
	PaintedAt = AskedAt;
	Painting.reset();
	++Repaints;
	PaintCost += Painted->Painted;
	WorstPaint = FMath::Max(WorstPaint, Painted->Painted);
	for (int32 K = 0; K < Pixels.Num(); ++K)
	{
		const uint8_t* From = &Painted->Rgba[static_cast<size_t>(K) * 4];
		Pixels[K] = FColor(From[0], From[1], From[2], From[3]);
	}
	SeaWeatherParts::Hand(Picture.Get(), Pixels);
	UploadCost += FPlatformTime::Seconds() - Began;
}

// ---- The deck's far sheet and the haze band --------------------------------------------------------------------------

void FSeaWeather::LayDeckSheet()
{
	using namespace SeaWeatherParts;
	UMaterialInterface* Ink = Load(TEXT("/Game/Campaign/Materials/M_Highlight.M_Highlight"));
	if (!Ink || !Room.Owner || !Room.Root)
	{
		return;
	}
	const double Solid = SeaSky::FarSheetSolid(Sky);
	DeckSheetZ = 0.5 * (Sky.Base + Sky.Top);
	// (an annulus round the eye at the deck's middle: none within 14 km, where the drawn tiles still stand, all of it
	// from 20 km, where they have thinned into the haze (AirSky::Reach), out past the sea's far edge)
	const double U = Room.UnitsPerMetre;
	static const double Rings[] = { 14000.0, 17000.0, 20000.0, 26000.0, 35000.0, 50000.0, 75000.0, 115000.0, 180000.0, 300000.0, 600000.0 };
	const int32 RingCount = UE_ARRAY_COUNT(Rings);
	const int32 Around = 72;
	TArray<FVector> Points;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FColor> Colours;
	for (int32 R = 0; R < RingCount; ++R)
	{
		const double In = FMath::SmoothStep(14000.0, 20000.0, Rings[R]);
		const uint8 Share = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt32(255.0 * Solid * In), 0, 255));
		for (int32 K = 0; K < Around; ++K)
		{
			const double Round = 2.0 * UE_DOUBLE_PI * K / Around;
			Points.Add(FVector(FMath::Cos(Round), FMath::Sin(Round), 0.0) * (Rings[R] * U));
			Normals.Add(FVector::UpVector);
			Colours.Add(FColor(255, 255, 255, Share));
		}
	}
	for (int32 R = 0; R + 1 < RingCount; ++R)
	{
		for (int32 K = 0; K < Around; ++K)
		{
			const int32 A = R * Around + K, B = R * Around + (K + 1) % Around, C = (R + 1) * Around + (K + 1) % Around, D = (R + 1) * Around + K;
			// (both ways round: seen from under the deck as well as from over it)
			Triangles.Append({ A, B, C, A, C, D, A, C, B, A, D, C });
		}
	}
	UProceduralMeshComponent* Mesh = NewObject<UProceduralMeshComponent>(Room.Owner, TEXT("SeaDeckSheet"));
	DeckSheet = Mesh;
	Mesh->bUseAsyncCooking = false;
	Mesh->SetupAttachment(Room.Root);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
	Mesh->SetTranslucentSortPriority(-45);
	Mesh->RegisterComponent();
	Mesh->CreateMeshSection(0, Points, Triangles, Normals, TArray<FVector2D>(), Colours, TArray<FProcMeshTangent>(), false);
	DeckSheetInk = UMaterialInstanceDynamic::Create(Ink, Room.Owner);
	if (UMaterialInstanceDynamic* Mid = DeckSheetInk.Get())
	{
		Mid->SetScalarParameterValue(TEXT("Brightness"), static_cast<float>(CloudInk * Lit));
		Mid->SetScalarParameterValue(TEXT("Opacity"), 1.0f);
		Mesh->SetMaterial(0, Mid);
	}
	DeckSheetShown = -2.0;
	UE_LOG(LogTheater, Display, TEXT("Sea weather: the deck carried on to the horizon as one sheet at %.0f m, %.2f solid (the sky reads %.2f)."),
		DeckSheetZ, Solid, Chosen.Cover);
}

void FSeaWeather::DeckSheetAt(const FVector& EyeWorld)
{
	UProceduralMeshComponent* Mesh = DeckSheet.Get();
	UMaterialInstanceDynamic* Mid = DeckSheetInk.Get();
	if (!Mesh || !Mid)
	{
		return;
	}
	Mesh->SetWorldLocation(FVector(EyeWorld.X, EyeWorld.Y, DeckSheetZ * Room.UnitsPerMetre));
	const double High = EyeWorld.Z / Room.UnitsPerMetre;
	const double Over = FMath::Clamp((High - Sky.Base) / FMath::Max(1.0, Sky.Top - Sky.Base), 0.0, 1.0);
	if (FMath::Abs(Over - DeckSheetShown) < 0.01)
	{
		return;
	}
	DeckSheetShown = Over;
	const AirSky::Rgb Here = SeaSky::FarSheetColour(Sky, High);
	Mid->SetVectorParameterValue(TEXT("Tint"), FLinearColor(Here.R, Here.G, Here.B, 1.0f));
}

void FSeaWeather::LayHazeBand()
{
	using namespace SeaWeatherParts;
	UMaterialInterface* Ink = Load(TEXT("/Game/Campaign/Materials/M_Highlight.M_Highlight"));
	if (!Ink || !Room.Owner || !Room.Root)
	{
		return;
	}
	SeaHalf = SeaReach * Room.UnitsPerMetre;
	UProceduralMeshComponent* Mesh = NewObject<UProceduralMeshComponent>(Room.Owner, TEXT("SeaHazeBand"));
	HazeBand = Mesh;
	Mesh->bUseAsyncCooking = false;
	Mesh->SetupAttachment(Room.Root);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
	Mesh->SetTranslucentSortPriority(-48);
	Mesh->RegisterComponent();
	HazeBandInk = UMaterialInstanceDynamic::Create(Ink, Room.Owner);
	if (UMaterialInstanceDynamic* Mid = HazeBandInk.Get())
	{
		Mid->SetScalarParameterValue(TEXT("Brightness"), static_cast<float>(Lit));
		Mid->SetVectorParameterValue(TEXT("Tint"), HazeInk);
		Mid->SetScalarParameterValue(TEXT("Opacity"), 0.0f);
	}
	HazeBandLaidAt = FVector(1e30, 1e30, 1e30);
	HazeBandShown = -1.0;
	UE_LOG(LogTheater, Display, TEXT("Sea weather: a haze band over the sea's far edge (the sea %.0f km each way), from an eye %.0f m up, all of it from %.0f m."),
		SeaReach / 1000.0, BandFrom, BandFull);
}

void FSeaWeather::HazeBandAt(const FVector& EyeWorld)
{
	using namespace SeaWeatherParts;
	UProceduralMeshComponent* Mesh = HazeBand.Get();
	UMaterialInstanceDynamic* Mid = HazeBandInk.Get();
	if (!Mesh || !Mid)
	{
		return;
	}
	const double U = Room.UnitsPerMetre;
	const double High = EyeWorld.Z / U;
	const double Want = FMath::Clamp((High - BandFrom) / (BandFull - BandFrom), 0.0, 1.0);
	if (FMath::Abs(Want - HazeBandShown) > 0.004)
	{
		HazeBandShown = Want;
		Mid->SetScalarParameterValue(TEXT("Opacity"), static_cast<float>(Want));
		Mesh->SetVisibility(Want > 0.0);
	}
	if (Want <= 0.0)
	{
		return;
	}
	// (laid again when the eye has gone 250 m along or 100 m up or down: the edge's angle under the eye moves little)
	const FVector Moved = EyeWorld - HazeBandLaidAt;
	if (FVector2D(Moved.X, Moved.Y).Size() < 250.0 * U && FMath::Abs(Moved.Z) < 100.0 * U)
	{
		return;
	}
	HazeBandLaidAt = EyeWorld;
	Mesh->SetWorldLocation(FVector(EyeWorld.X, EyeWorld.Y, 0.0));
	// Column by column round the eye: how far the sea's edge lies that way, and so how far under the level it shows.
	// The band stands at 0.6 of that distance, from the sea up through the edge and thinning to none 7 degrees above it
	// (the air battle's own rows).
	struct FRow
	{
		double Over;
		uint8 Share;
	};
	static const FRow Rows[] = { { 0.0, 0 }, { 0.0, 150 }, { -0.1, 255 }, { 0.4, 255 }, { 1.2, 180 }, { 2.2, 108 }, { 3.4, 52 }, { 5.0, 16 }, { 7.0, 0 } };
	const int32 RowCount = UE_ARRAY_COUNT(Rows);
	const int32 Around = 64;
	const FVector Middle = Room.Owner ? Room.Owner->GetActorTransform().TransformPosition(World(MiddleEast, MiddleNorth, 0.0)) : World(MiddleEast, MiddleNorth, 0.0);
	const double Ex = EyeWorld.X - Middle.X, Ey = EyeWorld.Y - Middle.Y;
	TArray<FVector> Points;
	TArray<FColor> Colours;
	Points.Reserve(Around * RowCount);
	Colours.Reserve(Around * RowCount);
	for (int32 K = 0; K < Around; ++K)
	{
		const double Round = 2.0 * UE_DOUBLE_PI * K / Around;
		const double Dx = FMath::Cos(Round), Dy = FMath::Sin(Round);
		double Out = 1e18;
		if (FMath::Abs(Dx) > 1e-9)
		{
			Out = FMath::Min(Out, ((Dx > 0.0 ? SeaHalf : -SeaHalf) - Ex) / Dx);
		}
		if (FMath::Abs(Dy) > 1e-9)
		{
			Out = FMath::Min(Out, ((Dy > 0.0 ? SeaHalf : -SeaHalf) - Ey) / Dy);
		}
		const double Far = FMath::Max(Out, 20000.0 * U) / U;
		const double Stand = 0.6 * Far;
		const double Edge = -FMath::Atan2(FMath::Max(High, 1.0), Far);
		const double Foot = -FMath::Atan2(FMath::Max(High, 1.0), Stand);
		for (int32 R = 0; R < RowCount; ++R)
		{
			double Angle = Edge + FMath::DegreesToRadians(Rows[R].Over);
			if (R == 0)
			{
				Angle = Foot;
			}
			else if (R == 1)
			{
				Angle = 0.5 * (Foot + Edge + FMath::DegreesToRadians(Rows[2].Over));
			}
			const double Z = FMath::Max(0.0, High + Stand * FMath::Tan(Angle));
			Points.Add(FVector(Dx * Stand * U, Dy * Stand * U, Z * U));
			Colours.Add(FColor(255, 255, 255, Rows[R].Share));
		}
	}
	if (Mesh->GetNumSections() == 0)
	{
		TArray<int32> Triangles;
		Triangles.Reserve(Around * (RowCount - 1) * 12);
		for (int32 K = 0; K < Around; ++K)
		{
			const int32 K2 = (K + 1) % Around;
			for (int32 R = 0; R + 1 < RowCount; ++R)
			{
				const int32 A = K * RowCount + R, B = K2 * RowCount + R, C = K2 * RowCount + R + 1, D = K * RowCount + R + 1;
				// (both ways round: the band is seen from inside, whichever way the engine takes as its face)
				Triangles.Append({ A, B, C, A, C, D, A, C, B, A, D, C });
			}
		}
		TArray<FVector> Normals;
		Normals.Init(FVector::UpVector, Points.Num());
		Mesh->CreateMeshSection(0, Points, Triangles, Normals, TArray<FVector2D>(), Colours, TArray<FProcMeshTangent>(), false);
		Mesh->SetMaterial(0, HazeBandInk.Get());
	}
	else
	{
		Mesh->UpdateMeshSection(0, Points, TArray<FVector>(), TArray<FVector2D>(), Colours, TArray<FProcMeshTangent>());
	}
}

// ---- The clouds' shade ----------------------------------------------------------------------------------------------

void FSeaWeather::LayShade()
{
	UDirectionalLightComponent* Lamp = Room.Key.Get();
	if (!Lamp || !Room.Owner)
	{
		return;
	}
	UMaterialInterface* Ink = SeaWeatherParts::Load(TEXT("/Game/Campaign/Materials/M_DeckShade.M_DeckShade"));
	if (!Ink)
	{
		UE_LOG(LogTheater, Warning, TEXT("Sea weather: M_DeckShade is missing (run Tools/build_content.sh): no cloud shade on the sea."));
		return;
	}
	const FVector Sun = Room.Sun.GetSafeNormal();
	Shade = SeaSky::ShadeOf(Sky, SeaSky::Vec(Sun.Y, Sun.X, Sun.Z), SeaSky::Vec(), 256);
	if (Shade.Side <= 0)
	{
		return;
	}
	// (the picture laid on the world's own axes: the scene's x is the board's y, so each row of the picture is a column
	// of the shade's)
	const int32 Side = Shade.Side;
	TArray<FColor> Lights;
	Lights.SetNumUninitialized(Side * Side);
	for (int32 Row = 0; Row < Side; ++Row)
	{
		for (int32 Col = 0; Col < Side; ++Col)
		{
			const uint8 Light = Shade.Light[static_cast<size_t>(Col) * static_cast<size_t>(Side) + static_cast<size_t>(Row)];
			Lights[Row * Side + Col] = FColor(Light, Light, Light, 255);
		}
	}
	ShadeSheet.Reset(CampaignTextures::Create(Side, Side, Lights, false, true,
		MakeUniqueObjectName(GetTransientPackage(), UTexture2D::StaticClass(), TEXT("SeaCloudShade"))));
	if (!ShadeSheet.IsValid())
	{
		return;
	}
	const double U = Room.UnitsPerMetre;
	const FTransform& Here = Room.Owner->GetActorTransform();
	const FVector Corner = Here.TransformPosition(World(Shade.Origin.X, Shade.Origin.Y, 0.0));
	const FVector Middle = Here.TransformPosition(World(0.0, 0.0, Shade.Z));
	const FVector Tops = Here.TransformPosition(World(0.0, 0.0, Shade.Top));
	FVector SunWay = Sun;
	SunWay.Z = FMath::Max(0.05, SunWay.Z);
	ShadeInk = UMaterialInstanceDynamic::Create(Ink, Room.Owner);
	UMaterialInstanceDynamic* Mid = ShadeInk.Get();
	if (!Mid)
	{
		return;
	}
	Mid->SetTextureParameterValue(TEXT("Shade"), ShadeSheet.Get());
	Mid->SetVectorParameterValue(TEXT("SunWay"), FLinearColor(static_cast<float>(SunWay.X), static_cast<float>(SunWay.Y), static_cast<float>(SunWay.Z), 0.0f));
	Mid->SetVectorParameterValue(TEXT("Origin"), FLinearColor(static_cast<float>(Corner.X), static_cast<float>(Corner.Y), 0.0f, 0.0f));
	Mid->SetScalarParameterValue(TEXT("Period"), static_cast<float>(Shade.Period * U));
	Mid->SetScalarParameterValue(TEXT("DeckZ"), static_cast<float>(Middle.Z));
	Mid->SetScalarParameterValue(TEXT("DeckTop"), static_cast<float>(Tops.Z));
	Mid->SetScalarParameterValue(TEXT("Fade"), static_cast<float>(100.0 * U));
	// (the lamp's light function as it was, given back in GiveBack: the plan's step 11(i) keep-and-give-back rule)
	KeptKey.Function = Lamp->LightFunctionMaterial;
	KeptKey.Fade = Lamp->LightFunctionFadeDistance;
	KeptKey.Dark = Lamp->DisabledBrightness;
	Lamp->SetLightFunctionMaterial(Mid);
	// (drawn to the haze's far end and past it; where it is not drawn, the clouds' mean)
	Lamp->SetLightFunctionFadeDistance(static_cast<float>(400000.0 * U));
	Lamp->SetLightFunctionDisabledBrightness(static_cast<float>(Shade.Mean()));
	bShade = true;
	UE_LOG(LogTheater, Display, TEXT("Sea weather: the clouds' shade on the sun, %d squares a side over %.0f km, %.2f of the sun under a cloud, %.2f on the whole; read at %.0f m, none over %.0f m."),
		Side, Shade.Period / 1000.0, Shade.Through, Shade.Mean(), Shade.Z, Shade.Top);
}

double FSeaWeather::CloudShade() const
{
	return bShade && Shade.Side > 0 ? FMath::Clamp(1.0 - Shade.Mean(), 0.0, 1.0) : 0.0;
}

AirSky::Lighting FSeaWeather::LightShares(double EyeZMetres) const
{
	if (!Chosen.bLaid)
	{
		return AirSky::Lighting();
	}
	const double Under = Chosen.Day.bDeck && Sky.Top > Sky.Base ? FMath::Clamp((Sky.Top - EyeZMetres) / (Sky.Top - Sky.Base), 0.0, 1.0) : 0.0;
	AirSky::Lighting Day = SeaSky::LightOf(Chosen, Under); // (a day's light is the cover it reads as, not what it is laid at)
	// (with the shade on the key the sun is taken away point by point instead, the eye's own height no matter)
	Day.Sun = bShade ? 1.0 : Day.Sun;
	return Day;
}

// ---- The far rain ---------------------------------------------------------------------------------------------------

void FSeaWeather::LayRain()
{
	using namespace SeaWeatherParts;
	Rain = SeaSky::RainUnder(Sky, Chosen.Rain, Seed);
	Wet = SeaSky::SquallOf(Rain);
	if (Rain.Points.empty() || !Room.Owner || !Room.Root)
	{
		return;
	}
	UMaterialInterface* Ink = Load(TEXT("/Game/Campaign/Materials/M_Highlight.M_Highlight"));
	if (!Ink)
	{
		return;
	}
	TArray<FVector> Points;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FColor> Colours;
	Points.Reserve(static_cast<int32>(Rain.Points.size()));
	for (size_t K = 0; K < Rain.Points.size(); ++K)
	{
		Points.Add(World(Rain.Points[K].X, Rain.Points[K].Y, Rain.Points[K].Z));
		Normals.Add(FVector::UpVector);
		Colours.Add(FColor(255, 255, 255, Rain.Alpha[K]));
	}
	Triangles.Reserve(static_cast<int32>(Rain.Triangles.size()));
	for (const int Each : Rain.Triangles)
	{
		Triangles.Add(Each);
	}
	UProceduralMeshComponent* Mesh = NewObject<UProceduralMeshComponent>(Room.Owner, TEXT("SeaRainCurtains"));
	Curtains = Mesh;
	Mesh->bUseAsyncCooking = false;
	Mesh->SetupAttachment(Room.Root);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
	Mesh->SetTranslucentSortPriority(-12);
	Mesh->RegisterComponent();
	Mesh->CreateMeshSection(0, Points, Triangles, Normals, TArray<FVector2D>(), Colours, TArray<FProcMeshTangent>(), false);
	CurtainsInk = UMaterialInstanceDynamic::Create(Ink, Room.Owner);
	if (UMaterialInstanceDynamic* Mid = CurtainsInk.Get())
	{
		Mid->SetScalarParameterValue(TEXT("Brightness"), static_cast<float>(Lit));
		Mid->SetScalarParameterValue(TEXT("Opacity"), 1.0f);
		Mid->SetVectorParameterValue(TEXT("Tint"), RainInk);
		Mesh->SetMaterial(0, Mid);
	}
	UE_LOG(LogTheater, Display, TEXT("Sea weather: %d far rain curtains under the darkest clouds, %.0f to %.0f km out, rain %.2f%s."),
		static_cast<int32>(Rain.Curtains.size()), SeaSky::CurtainsFrom / 1000.0, SeaSky::CurtainsTo / 1000.0, Rain.Heavy,
		Wet.bOn ? *FString::Printf(TEXT("; the heaviest squall %.0f m across on the sea"), Wet.R) : TEXT(""));
}

// ---- The day's light ------------------------------------------------------------------------------------------------

void FSeaWeather::SetDayLight(double Share)
{
	Lit = FMath::Clamp(Share, 0.0, 4.0);
	SetLightEverywhere();
}

void FSeaWeather::SetLightEverywhere()
{
	if (UMaterialInstanceDynamic* Mid = DomeInk.Get())
	{
		Mid->SetScalarParameterValue(TEXT("DayLight"), static_cast<float>(Lit));
	}
	if (UMaterialInstanceDynamic* Mid = DeckSheetInk.Get())
	{
		Mid->SetScalarParameterValue(TEXT("Brightness"), static_cast<float>(SeaWeatherParts::CloudInk * Lit));
	}
	if (UMaterialInstanceDynamic* Mid = HazeBandInk.Get())
	{
		Mid->SetScalarParameterValue(TEXT("Brightness"), static_cast<float>(Lit));
	}
	if (UMaterialInstanceDynamic* Mid = CurtainsInk.Get())
	{
		Mid->SetScalarParameterValue(TEXT("Brightness"), static_cast<float>(Lit));
	}
}

// ---- Every frame ----------------------------------------------------------------------------------------------------

void FSeaWeather::Tick(const FVector& EyeWorld, bool bCut, double BattleSeconds)
{
	if (!bOn)
	{
		return;
	}
	if (UTheaterStillMesh* Mesh = Dome.Get())
	{
		// (the dome stands round the eye, its own middle at the sea: at 150 km the eye's own height is nothing)
		Mesh->SetWorldLocation(FVector(EyeWorld.X, EyeWorld.Y, 0.0));
	}
	TakePaint();
	if (UMaterialInstanceDynamic* Mid = DomeInk.Get())
	{
		// (the clouds carried downwind since the picture was painted, so they drift on smoothly between paints; the dome
		// itself never moves, and a deck never drifts at all. The world's x is the board's north, its y the board's east)
		const SeaSky::Vec Was = Sky.DriftAt(PaintedAt), Now = Sky.DriftAt(BattleSeconds);
		const double U = Room.UnitsPerMetre;
		Mid->SetVectorParameterValue(TEXT("Drift"),
			FLinearColor(static_cast<float>((Now.Y - Was.Y) * U), static_cast<float>((Now.X - Was.X) * U), 0.0f, 0.0f));
	}
	const bool bMoved = FVector(EyeWorld - AskedEye).Size() > SeaWeatherParts::PaintMoved * Room.UnitsPerMetre;
	bCutLater = bCutLater || bCut;
	if (!bWaiting && (bCutLater || bMoved || BattleSeconds - AskedAt > SeaWeatherParts::PaintEvery))
	{
		bCutLater = false;
		AskPaint(EyeWorld, BattleSeconds);
	}
	DeckSheetAt(EyeWorld);
	HazeBandAt(EyeWorld);
}

void FSeaWeather::GiveBack()
{
	if (Job.IsValid())
	{
		Job.Wait();
		Job.Reset();
	}
	bWaiting = false;
	UDirectionalLightComponent* Lamp = Room.Key.Get();
	if (bShade && Lamp)
	{
		// (the key lamp's light function as it was before the clouds: the room is given back whole)
		Lamp->SetLightFunctionMaterial(KeptKey.Function.Get());
		Lamp->SetLightFunctionFadeDistance(KeptKey.Fade);
		Lamp->SetLightFunctionDisabledBrightness(KeptKey.Dark);
		Lamp->MarkRenderStateDirty();
	}
	bShade = false;
	bOn = false;
	auto Away = [](TWeakObjectPtr<UProceduralMeshComponent>& Each)
	{
		if (UProceduralMeshComponent* Mesh = Each.Get())
		{
			Mesh->DestroyComponent();
		}
		Each.Reset();
	};
	Away(DeckSheet);
	Away(HazeBand);
	Away(Curtains);
	if (UTheaterStillMesh* Mesh = Dome.Get())
	{
		Mesh->DestroyComponent();
	}
	Dome.Reset();
	DomeInk.Reset();
	DeckSheetInk.Reset();
	HazeBandInk.Reset();
	CurtainsInk.Reset();
	ShadeInk.Reset();
	Picture.Reset();
	ShadeSheet.Reset();
	Painted.reset();
	Painting.reset();
	Brush.reset();
}

void FSeaWeather::End()
{
	GiveBack();
}

double FSeaWeather::OverviewHeight() const
{
	return SeaSky::OverviewHeight(bOn && Switches.Draw != SeaSky::Clouds::Off && Chosen.Day.bDeck, Sky.Base);
}

bool FSeaWeather::DeckDrawn(double& OutBase) const
{
	OutBase = Sky.Base;
	return bOn && Switches.Draw != SeaSky::Clouds::Off && Chosen.Day.bDeck;
}

void FSeaWeather::SayFrameTimes() const
{
	if (Repaints <= 0)
	{
		return;
	}
	UE_LOG(LogTheater, Display, TEXT("Sea weather: the cloud dome painted %d times, %.1f ms each on a pool thread (worst %.1f), %.1f ms handed to the chip in all; %d frames waited for a paint."),
		Repaints, 1000.0 * PaintCost / Repaints, 1000.0 * WorstPaint, 1000.0 * UploadCost, Waits);
}
