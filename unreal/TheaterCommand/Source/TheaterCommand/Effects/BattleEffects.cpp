#include "Effects/BattleEffects.h"

#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
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

#include <string_view>

// ---------------------------------------------------------------------------------------------------------------------
// The recipe

bool FBattleFxRecipe::Load(const FString& Path)
{
	FString Text;
	TSharedPtr<FJsonObject> File;
	if (!FFileHelper::LoadFileToString(Text, *Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), File) || !File.IsValid())
	{
		return false;
	}
	Numbers.clear();
	Words.clear();
	Walk(MakeShared<FJsonValueObject>(File), std::string());
	return true;
}

void FBattleFxRecipe::Walk(const TSharedPtr<FJsonValue>& Value, const std::string& Path)
{
	if (!Value.IsValid())
	{
		return;
	}
	switch (Value->Type)
	{
	case EJson::Object:
	{
		const TSharedPtr<FJsonObject> Object = Value->AsObject();
		if (Object.IsValid())
		{
			for (const auto& Field : Object->Values) // (keyed by FString or, in newer engines, FSharedString)
			{
				const std::string Name(TCHAR_TO_UTF8(*Field.Key));
				Walk(Field.Value, Path.empty() ? Name : Path + "." + Name);
			}
		}
		break;
	}
	case EJson::Array:
	{
		const TArray<TSharedPtr<FJsonValue>>& List = Value->AsArray();
		for (int32 Index = 0; Index < List.Num(); ++Index)
		{
			Walk(List[Index], Path + "." + std::to_string(Index));
		}
		break;
	}
	case EJson::Number:
		Numbers[Path] = Value->AsNumber();
		break;
	case EJson::Boolean:
		Numbers[Path] = Value->AsBool() ? 1.0 : 0.0;
		break;
	case EJson::String:
		Words[Path] = std::string(TCHAR_TO_UTF8(*Value->AsString()));
		break;
	default:
		break;
	}
}

bool FBattleFxRecipe::Find(const char* Path, double& Out) const
{
	if (!Path)
	{
		return false;
	}
	const auto Found = Numbers.find(std::string_view(Path));
	if (Found == Numbers.end())
	{
		return false;
	}
	Out = Found->second;
	return true;
}

bool FBattleFxRecipe::FindWord(const char* Path, std::string& Out) const
{
	if (!Path)
	{
		return false;
	}
	const auto Found = Words.find(std::string_view(Path));
	if (Found == Words.end())
	{
		return false;
	}
	Out = Found->second;
	return true;
}

// ---------------------------------------------------------------------------------------------------------------------
// The effects

FBattleEffects::FBattleEffects() = default;

// (nothing of the engine's is touched here: it may be run while the world is being torn down; End does that)
FBattleEffects::~FBattleEffects() = default;

void FBattleEffects::End()
{
	if (Mesh.IsValid())
	{
		Mesh->DestroyComponent();
	}
	for (TWeakObjectPtr<UPointLightComponent>& Lamp : LampLights)
	{
		if (Lamp.IsValid())
		{
			Lamp->DestroyComponent();
		}
		Lamp.Reset();
	}
	Mesh.Reset();
	Ink.Reset();
	Holder.Reset();
	Core.Clear();
	TestShots.Reset();
	TestStart = -1.0;
	Room = 0;
	RoomSpare = 0;
	LastSent = 0;
	bShown = false;
}

double FBattleEffects::GroundHeight(void* Context, double X, double Y)
{
	const FBattleEffects* Self = static_cast<const FBattleEffects*>(Context);
	if (!Self || !Self->GroundUnder)
	{
		return 0.0;
	}
	const double U = Self->UnitsPerMetre;
	return Self->GroundUnder(FVector(X * U, Y * U, 0.0)).Z / U;
}

TheaterFx::Vec FBattleEffects::ToFx(const FVector& World) const
{
	return TheaterFx::Vec(World.X / UnitsPerMetre, World.Y / UnitsPerMetre, World.Z / UnitsPerMetre);
}

TheaterFx::Vec FBattleEffects::ToFxWay(const FVector& Way) const
{
	return TheaterFx::Vec(Way.X, Way.Y, Way.Z);
}

double FBattleEffects::Time() const
{
	// (one clock for everything: the time Tick was last given, so what happens and what is drawn always agree)
	return Clock;
}

bool FBattleEffects::Begin(USceneComponent* Parent, double InUnitsPerMetre, TFunction<FVector(const FVector&)> InGroundUnder)
{
	if (IsReady())
	{
		return true;
	}
	if (!Parent || !Parent->GetOwner() || InUnitsPerMetre <= 0.0)
	{
		UE_LOG(LogTheater, Warning, TEXT("Battle effects: no parent to hang on, or no scale; nothing will be drawn"));
		return false;
	}
	const FString Path = FPaths::ProjectContentDir() / TEXT("Effects") / TEXT("battle-fx.json");
	if (!Book.Load(Path))
	{
		UE_LOG(LogTheater, Warning, TEXT("Battle effects: the recipe is missing or unreadable (%s); nothing will be drawn"), *Path);
		return false;
	}
	UMaterialInterface* Highlight = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Campaign/Materials/M_Highlight.M_Highlight"));
	if (!Highlight)
	{
		UE_LOG(LogTheater, Warning, TEXT("Battle effects: M_Highlight is missing (run Tools/build_content.sh); nothing will be drawn"));
		return false;
	}
	Holder = Parent;
	UnitsPerMetre = InUnitsPerMetre;
	GroundUnder = MoveTemp(InGroundUnder);
	AActor* Owner = Parent->GetOwner();

	// The puffs: one mesh, in world places (it takes no turn or scale from its parent), never in shadow or collision.
	UProceduralMeshComponent* Made = NewObject<UProceduralMeshComponent>(Owner, MakeUniqueObjectName(Owner, UProceduralMeshComponent::StaticClass(), TEXT("BattleEffects")));
	Made->bUseAsyncCooking = false;
	Made->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Made->SetCastShadow(false);
	Made->SetupAttachment(Parent);
	Made->SetUsingAbsoluteLocation(true);
	Made->SetUsingAbsoluteRotation(true);
	Made->SetUsingAbsoluteScale(true);
	Made->RegisterComponent();
	Made->SetWorldTransform(FTransform::Identity);
	Mesh = Made;
	// (its colours are the recipe's own: Brightness 1, where the map's lines take 1.6)
	UMaterialInstanceDynamic* Instance = UMaterialInstanceDynamic::Create(Highlight, Made);
	Instance->SetScalarParameterValue(TEXT("Brightness"), 1.0f);
	Made->SetMaterial(0, Instance); // (held here, in the mesh's own list, before it has any section: Ink only watches it)
	Ink = Instance;

	// The flashes' lamps: a few, shadowless, out until a flash lights one.
	Core.Setup(&Book, &FBattleEffects::GroundHeight, this, 3u);
	double Wanted = 2.0, Candela = 2000.0;
	Book.Find("game.max_lamps", Wanted);
	Book.Find("game.lamp_candela_at_full", Candela);
	LampsInUse = FMath::Clamp(static_cast<int32>(Wanted), 0, MostLamps);
	LampCandela = Candela * FMath::Square(UnitsPerMetre / 100.0); // (the recipe's candelas are at 100 units a metre)
	for (int32 Index = 0; Index < LampsInUse; ++Index)
	{
		UPointLightComponent* Lamp = NewObject<UPointLightComponent>(Owner);
		Lamp->SetMobility(EComponentMobility::Movable);
		Lamp->SetupAttachment(Parent);
		Lamp->SetUsingAbsoluteLocation(true);
		Lamp->SetCastShadows(false);
		Lamp->IntensityUnits = ELightUnits::Candelas;
		Lamp->bUseInverseSquaredFalloff = true;
		Lamp->SetSpecularScale(0.1f);
		Lamp->SetIntensity(0.0f);
		Lamp->RegisterComponent();
		Lamp->SetVisibility(false);
		LampLights[Index] = Lamp;
	}
	const UWorld* World = Parent->GetWorld();
	Clock = World ? World->GetTimeSeconds() : 0.0; // (until the first Tick)
	UE_LOG(LogTheater, Log, TEXT("Battle effects ready: %.2f world units a metre, room for %d puffs, %d lamps"), UnitsPerMetre, Core.Room(), LampsInUse);
	return true;
}

bool FBattleEffects::IsReady() const
{
	return Mesh.IsValid() && Ink.IsValid();
}

std::string FBattleEffects::Named(const FString& Text)
{
	// (the recipe's rows are lower case and its look-ups exact: "Earth" or "HMG" from a caller finds its row)
	return std::string(TCHAR_TO_UTF8(*Text.ToLower()));
}

void FBattleEffects::Shell(const FVector& WorldAt, const FString& Calibre, const FString& Ground, bool bOnArmour)
{
	if (IsReady())
	{
		const std::string Size = Core.SizeOfCalibre(Named(Calibre).c_str());
		Core.Shell(Time(), ToFx(WorldAt), Size.c_str(), Named(Ground).c_str(), bOnArmour);
	}
}

void FBattleEffects::ShellOfSize(const FVector& WorldAt, const FString& Size, const FString& Ground, bool bOnArmour)
{
	if (IsReady())
	{
		Core.Shell(Time(), ToFx(WorldAt), Named(Size).c_str(), Named(Ground).c_str(), bOnArmour);
	}
}

void FBattleEffects::Skip(const FVector& WorldAt, const FVector& Heading, const FString& Ground)
{
	if (IsReady())
	{
		Core.Skip(Time(), ToFx(WorldAt), ToFxWay(Heading), Named(Ground).c_str());
	}
}

void FBattleEffects::Muzzle(const FVector& WorldAt, const FVector& Facing, const FString& Gun)
{
	if (IsReady())
	{
		Core.Muzzle(Time(), ToFx(WorldAt), ToFxWay(Facing), Named(Gun).c_str());
	}
}

void FBattleEffects::Shots(int32 Gun, const FVector& From, const FVector& To, const FString& Weapon, int32 Rounds, float PerSecond, const FString& Ground)
{
	if (IsReady())
	{
		Core.Shots(Time(), Gun, ToFx(From), ToFx(To), Named(Weapon).c_str(), Rounds, PerSecond, Named(Ground).c_str());
	}
}

void FBattleEffects::AirGun(double Now, int32 Gun, const FVector& Muzzle, const FVector& Aim, const FVector& ShooterVelocity, const FString& Kind, float RatePerMinute, float MuzzleSpeed)
{
	if (IsReady())
	{
		// (velocities are world units a second, as places are: the same ToFx turns both to metres)
		Core.AirGun(Now, Gun, ToFx(Muzzle), ToFx(Aim), ToFx(ShooterVelocity), Named(Kind).c_str(), RatePerMinute, MuzzleSpeed / UnitsPerMetre);
	}
}

void FBattleEffects::AirGunStop(int32 Gun)
{
	if (IsReady())
	{
		Core.AirGunStop(Gun);
	}
}

void FBattleEffects::AirHit(double Now, const FVector& At, const FVector& Normal, const FVector& AircraftVelocity, const FString& Kind)
{
	if (IsReady())
	{
		Core.AirHit(Now, ToFx(At), ToFxWay(Normal), ToFx(AircraftVelocity), Named(Kind).c_str());
	}
}

void FBattleEffects::TrailStart(double Now, int32 Id, const FVector& At, const FVector& Velocity, const FString& Kind, float SpanMetres)
{
	if (IsReady())
	{
		Core.TrailStart(Now, Id, ToFx(At), ToFx(Velocity), Named(Kind).c_str(), SpanMetres);
	}
}

void FBattleEffects::TrailMove(double Now, int32 Id, const FVector& At, const FVector& Velocity)
{
	if (IsReady())
	{
		Core.TrailMove(Now, Id, ToFx(At), ToFx(Velocity));
	}
}

void FBattleEffects::TrailStop(int32 Id)
{
	if (IsReady())
	{
		Core.TrailStop(Id);
	}
}

void FBattleEffects::AirFlak(double Now, const FVector& At, const FString& Kind)
{
	if (IsReady())
	{
		Core.AirFlak(Now, ToFx(At), Named(Kind).c_str());
	}
}

void FBattleEffects::Burn(const FVector& WorldAt, float AcrossMetres, float Seconds)
{
	if (IsReady())
	{
		Core.Burn(Time(), ToFx(WorldAt), AcrossMetres, Seconds);
	}
}

void FBattleEffects::OilFire(int32 Id, const FVector& WorldAt, float ReachMetres, const FString& Kind, double UntilSeconds, float SinceSeconds)
{
	if (IsReady())
	{
		Core.OilFire(Time(), Id, ToFx(WorldAt), ReachMetres, Named(Kind).c_str(), UntilSeconds, SinceSeconds);
	}
}

void FBattleEffects::OilStop(int32 Id)
{
	if (IsReady())
	{
		Core.OilStop(Time(), Id);
	}
}

void FBattleEffects::SetWind(const FVector& MetresPerSecond)
{
	Core.SetWind(ToFxWay(MetresPerSecond));
}

void FBattleEffects::UseAirDistance()
{
	if (IsReady())
	{
		Core.SetDrawBeyond(Core.AirBeyond());
	}
}

void FBattleEffects::SetSun(const FVector& TowardSun)
{
	Sun = TowardSun.GetSafeNormal(UE_SMALL_NUMBER, FVector(-0.45, -0.35, 0.82));
}

void FBattleEffects::Clear()
{
	Core.Clear();
	TestShots.Reset();
	TestStart = -1.0;
	Hide();
}

void FBattleEffects::MakeSection(int32 NewRoom, const FVector& Fill)
{
	// The mesh is made with room for so many puffs (the map smoke's shape: a middle and two rings, of game.puff_sides
	// points each, both faces of every triangle); after that only its points and colours change, until it needs more
	// room or much less.
	const int32 Ring = Core.Ring(), PerPuff = Core.PerPuff();
	TArray<int32> Triangles;
	Triangles.Reserve(NewRoom * Ring * 18);
	for (int32 Each = 0; Each < NewRoom; ++Each)
	{
		const int32 First = Each * PerPuff;
		for (int32 K = 0; K < Ring; ++K)
		{
			const int32 Next = (K + 1) % Ring;
			const int32 In0 = First + 1 + K, In1 = First + 1 + Next, Out0 = First + 1 + Ring + K, Out1 = First + 1 + Ring + Next;
			Triangles.Append({ First, In1, In0, First, In0, In1 }); // the heart
			Triangles.Append({ In0, In1, Out1, In0, Out1, In1 }); // and the rim round it
			Triangles.Append({ In0, Out1, Out0, In0, Out0, Out1 });
		}
	}
	Room = NewRoom;
	Points.Init(Fill, Room * PerPuff + 2); // (spare room at the battle, not at the world's origin, so the bounds stay tight)
	Colours.Init(FColor(0, 0, 0, 0), Room * PerPuff + 2);
	LastSent = 0;
	TArray<FVector> Normals;
	Normals.Init(FVector::UpVector, Points.Num());
	Mesh->CreateMeshSection(0, Points, Triangles, Normals, TArray<FVector2D>(), Colours, TArray<FProcMeshTangent>(), false);
	Mesh->SetMaterial(0, Ink.Get());
}

void FBattleEffects::Hide()
{
	if (bShown && Mesh.IsValid())
	{
		Mesh->SetVisibility(false);
	}
	bShown = false;
	for (int32 Index = 0; Index < LampsInUse; ++Index)
	{
		if (LampLights[Index].IsValid())
		{
			LampLights[Index]->SetVisibility(false);
		}
	}
}

FBattleJolt FBattleEffects::Tick(double Now, const FVector& Eye, const FRotator& Facing, float FieldOfViewDegrees, int32 ViewWidth, const FLinearColor& HourLight)
{
	FBattleJolt Jolt;
	if (!IsReady())
	{
		return Jolt;
	}
	Clock = Now;
	PlayTest(Now);
	const TheaterFx::Vec EyeAt = ToFx(Eye);
	if (bShake)
	{
		const TheaterFx::Battle::Jolt Shaken = Core.Shake(Now, EyeAt);
		Jolt.Offset = FVector(Shaken.Offset.X, Shaken.Offset.Y, Shaken.Offset.Z) * UnitsPerMetre;
		Jolt.RollDegrees = static_cast<float>(Shaken.Roll);
	}
	if (!Core.Busy(Now))
	{
		Hide();
		return Jolt;
	}

	const FRotationMatrix Looking(Facing);
	TheaterFx::Battle::View Sight;
	Sight.Eye = EyeAt;
	Sight.Forward = ToFxWay(Looking.GetUnitAxis(EAxis::X));
	Sight.Right = ToFxWay(Looking.GetUnitAxis(EAxis::Y));
	Sight.Up = ToFxWay(Looking.GetUnitAxis(EAxis::Z));
	const double HalfView = FMath::DegreesToRadians(FMath::Clamp(static_cast<double>(FieldOfViewDegrees), 5.0, 170.0) * 0.5);
	Sight.PixelsPerRadian = FMath::Max(1, ViewWidth) / (2.0 * FMath::Tan(HalfView));
	Sight.SmokeLight = TheaterFx::Rgb(HourLight.R, HourLight.G, HourLight.B);
	Sight.Sun = ToFxWay(Sun);
	const float SunUp = static_cast<float>(Core.DayOf(Sight.SmokeLight)); // (the sun's own light: full by day, none at night)
	Sight.SunLight = TheaterFx::Rgb(SunUp, SunUp, SunUp);
	const int32 Drawn = Core.Draw(Now, Sight);

	// (room kept in steps of 300 puffs, let down only after two seconds with fewer, as the map's smoke does)
	const int32 Step = 300;
	int32 Wanted = FMath::Min(FMath::DivideAndRoundUp(FMath::Max(Drawn, 1), Step) * Step, FMath::Max(Step, Core.Room()));
	if (Wanted < Room && ++RoomSpare < 120)
	{
		Wanted = Room;
	}
	else if (Wanted >= Room)
	{
		RoomSpare = 0;
	}
	const int32 PerPuff = Core.PerPuff();
	const std::vector<TheaterFx::Vec>& From = Core.Points();
	const std::vector<uint8_t>& Rgba = Core.Colours();
	if (Wanted != Room)
	{
		MakeSection(Wanted, Drawn > 0 ? FVector(From[0].X, From[0].Y, From[0].Z) * UnitsPerMetre : Eye);
	}
	// (what the core drew, and what it folded away from last frame: the rest of the room is folded already)
	const int32 Send = FMath::Min(Room, FMath::Max(Drawn, LastSent)) * PerPuff;
	FBox Whole(ForceInit);
	for (int32 Index = 0; Index < Send; ++Index)
	{
		const TheaterFx::Vec& P = From[static_cast<size_t>(Index)];
		Points[Index] = FVector(P.X, P.Y, P.Z) * UnitsPerMetre;
		const size_t C = static_cast<size_t>(Index) * 4;
		Colours[Index] = FColor(Rgba[C], Rgba[C + 1], Rgba[C + 2], Rgba[C + 3]); // (linear already, as ToFColor(false))
		if (Index < Drawn * PerPuff)
		{
			Whole += Points[Index];
		}
	}
	LastSent = FMath::Min(Drawn, Room);
	// (two points more that are no puff's: the corners of all the puffs, so that the mesh's bounds hold them)
	if (Whole.IsValid)
	{
		Points[Room * PerPuff] = Whole.Min;
		Points[Room * PerPuff + 1] = Whole.Max;
	}
	Mesh->UpdateMeshSection(0, Points, TArray<FVector>(), TArray<FVector2D>(), Colours, TArray<FProcMeshTangent>());
	if (!bShown)
	{
		Mesh->SetVisibility(true);
		bShown = true;
	}

	// the lamps: the strongest flashes now, out the moment they are spent
	TheaterFx::Battle::Lamp Lit[MostLamps];
	const int32 NLit = Core.Lamps(Now, Lit, LampsInUse, Core.DayOf(Sight.SmokeLight)); // (after Draw: a flash's lamp goes by the frames it is drawn in)
	// (-TheaterFxLampLog: a line for each frame a lamp is lit, and one for the first frame after with none)
	static const bool bLampLog = FParse::Param(FCommandLine::Get(), TEXT("TheaterFxLampLog"));
	if (bLampLog && (NLit > 0 || LampsLitBefore > 0))
	{
		FString Line = FString::Printf(TEXT("Battle effects lamps at %.3f s: %d lit"), Now, NLit);
		for (int32 Index = 0; Index < NLit; ++Index)
		{
			const TheaterFx::Battle::Lamp& L = Lit[Index];
			Line += FString::Printf(TEXT("; #%d at (%.0f, %.0f, %.0f) %.1f cd reach %.0f colour %.2f %.2f %.2f"), Index, L.At.X * UnitsPerMetre, L.At.Y * UnitsPerMetre, L.At.Z * UnitsPerMetre, L.Strength * LampCandela, L.Radius * UnitsPerMetre, L.Col.R, L.Col.G, L.Col.B);
		}
		UE_LOG(LogTheater, Log, TEXT("%s"), *Line);
	}
	LampsLitBefore = NLit;
	for (int32 Index = 0; Index < LampsInUse; ++Index)
	{
		UPointLightComponent* Lamp = LampLights[Index].Get();
		if (!Lamp)
		{
			continue;
		}
		if (Index >= NLit)
		{
			Lamp->SetVisibility(false);
			continue;
		}
		const TheaterFx::Battle::Lamp& L = Lit[Index];
		Lamp->SetWorldLocation(FVector(L.At.X, L.At.Y, L.At.Z) * UnitsPerMetre);
		Lamp->SetLightColor(FLinearColor(L.Col.R, L.Col.G, L.Col.B));
		Lamp->SetAttenuationRadius(static_cast<float>(L.Radius * UnitsPerMetre));
		Lamp->SetIntensity(static_cast<float>(L.Strength * LampCandela));
		Lamp->SetVisibility(true);
	}
	return Jolt;
}

bool FBattleEffects::WantsTest()
{
	return FParse::Param(FCommandLine::Get(), TEXT("TheaterBlastTest"));
}

void FBattleEffects::RunTest(const FVector& Centre)
{
	// The test barrage, laid out in metres round Centre and played again every 24 s: each calibre on earth in a row
	// across the view, then sand, snow and mud, a bomb further off, a hit on armour, a skip, guns firing tracer and a
	// wreck burning.
	TestCentre = Centre;
	TestStart = Time();
	TestShots.Reset();
	auto Add = [this](double When, int32 Kind, double X, double Y, const TCHAR* Size, const TCHAR* Ground)
	{
		FTestShot Shot;
		Shot.When = When;
		Shot.Kind = Kind;
		Shot.Where = FVector(X, Y, 0.0);
		Shot.Size = Size;
		Shot.Ground = Ground;
		TestShots.Add(Shot);
	};
	Add(0.5, 0, -30.0, 0.0, TEXT("light"), TEXT("earth"));
	Add(1.5, 0, -18.0, 4.0, TEXT("mortar"), TEXT("earth"));
	Add(2.7, 0, -4.0, -2.0, TEXT("gun75"), TEXT("earth"));
	Add(4.0, 0, 12.0, 3.0, TEXT("how105"), TEXT("earth"));
	Add(5.6, 0, 32.0, 6.0, TEXT("how150"), TEXT("earth"));
	Add(7.5, 0, -20.0, 30.0, TEXT("how105"), TEXT("sand"));
	Add(8.6, 0, 0.0, 34.0, TEXT("how105"), TEXT("snow"));
	Add(9.7, 0, 20.0, 30.0, TEXT("how105"), TEXT("mud"));
	Add(11.0, 0, 40.0, 90.0, TEXT("bomb250"), TEXT("earth"));
	Add(13.5, 1, -8.0, 14.0, TEXT("gun75"), TEXT("earth")); // on armour
	Add(14.2, 2, 6.0, 12.0, TEXT(""), TEXT("earth")); // a skip
	Add(15.0, 3, -40.0, -6.0, TEXT("tank_gun"), TEXT("")); // a tank gun firing toward the row
	Add(15.5, 4, -36.0, -10.0, TEXT("mg"), TEXT("")); // a machine gun's burst, tracer
	Add(16.0, 5, 24.0, 18.0, TEXT(""), TEXT("")); // a wreck burning
	Add(18.0, 4, -36.0, -10.0, TEXT("mg"), TEXT(""));
	Add(19.0, 6, 0.0, 60.0, TEXT("heavy"), TEXT("")); // a heavy flak burst hanging 60 m up over the far row
	// (and behind it all, for the whole test, two oil fires already burning a minute and a half: a slick's black column
	// 900 m off and a burning ship's 2.5 km off, as a sea battle's lost ships stand on its horizon)
	auto Far = [this, &Centre](double X, double Y)
	{
		const FVector Flat = Centre + FVector(X, Y, 0.0) * UnitsPerMetre;
		return GroundUnder ? GroundUnder(Flat) : Flat;
	};
	OilFire(9001, Far(-250.0, 900.0), 100.0f, TEXT("sunk"), 0.0, 90.0f);
	OilFire(9002, Far(600.0, 2500.0), 70.0f, TEXT("burning"), 0.0, 90.0f);
}

void FBattleEffects::PlayTest(double Now)
{
	if (TestStart < 0.0 || TestShots.Num() == 0)
	{
		return;
	}
	const double Cycle = 24.0;
	const double Into = Now - TestStart;
	for (FTestShot& Shot : TestShots)
	{
		if (Shot.When > Into)
		{
			continue;
		}
		Shot.When += Cycle; // (the next time round)
		const FVector Flat = TestCentre + Shot.Where * UnitsPerMetre;
		const FVector Where = GroundUnder ? GroundUnder(Flat) : Flat;
		switch (Shot.Kind)
		{
		case 0:
			ShellOfSize(Where, Shot.Size, Shot.Ground, false);
			break;
		case 1:
			ShellOfSize(Where + FVector(0.0, 0.0, 1.2 * UnitsPerMetre), Shot.Size, Shot.Ground, true);
			break;
		case 2:
			Skip(Where, FVector(1.0, 0.3, 0.0), Shot.Ground);
			break;
		case 3:
		{
			const FVector Gun = Where + FVector(0.0, 0.0, 2.0 * UnitsPerMetre);
			const FVector Target = TestCentre + FVector(12.0, 3.0, 1.0) * UnitsPerMetre;
			Muzzle(Gun, (Target - Gun).GetSafeNormal(), Shot.Size);
			break;
		}
		case 4:
		{
			const FVector Gun = Where + FVector(0.0, 0.0, 0.6 * UnitsPerMetre);
			const FVector Target = TestCentre + FVector(20.0, 20.0, 0.5) * UnitsPerMetre;
			Shots(13, Gun, Target, Shot.Size, 25, 15.0f, TEXT("earth")); // (gun 13's belt starts on a tracer, so the first frames show one)
			break;
		}
		case 5:
			Burn(Where, 3.0f, 12.0f);
			break;
		case 6:
			AirFlak(Now, Where + FVector(0.0, 0.0, 60.0 * UnitsPerMetre), Shot.Size);
			break;
		default:
			break;
		}
	}
}
