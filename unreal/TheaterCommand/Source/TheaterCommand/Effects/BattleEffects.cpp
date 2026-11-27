#include "Effects/BattleEffects.h"

#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/Scene.h"
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

bool FBattleFxRecipe::Load(const FString& Path, const TArray<FString>& Over)
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
	// (a scene's own look laid over, file by file: the numbers it names replace the file's, the rest stay)
	for (const FString& OverPath : Over)
	{
		FString OverText;
		TSharedPtr<FJsonObject> OverFile;
		if (!FFileHelper::LoadFileToString(OverText, *OverPath) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(OverText), OverFile) || !OverFile.IsValid())
		{
			UE_LOG(LogTheater, Warning, TEXT("Battle effects: %s is missing or unreadable; the recipe stays as it is"), *OverPath);
			continue;
		}
		const size_t Before = Numbers.size();
		Walk(MakeShared<FJsonValueObject>(OverFile), std::string());
		UE_LOG(LogTheater, Display, TEXT("Battle effects: %s laid over the recipe (%d numbers added to the file's)"), *FPaths::GetCleanFilename(OverPath), static_cast<int32>(Numbers.size() - Before));
	}
	// (-TheaterFxSet=path=number,path=number: numbers set over the file's, for a switch flipped at launch)
	FString Sets;
	if (FParse::Value(FCommandLine::Get(), TEXT("TheaterFxSet="), Sets, false))
	{
		TArray<FString> Parts;
		Sets.ParseIntoArray(Parts, TEXT(","), true);
		for (const FString& Part : Parts)
		{
			FString Key, Val;
			const bool bSplit = Part.Split(TEXT("="), &Key, &Val);
			Key.TrimStartAndEndInline();
			Val.TrimStartAndEndInline();
			if (bSplit && !Key.IsEmpty() && !Val.IsEmpty() && FCString::IsNumeric(*Val)) // (IsNumeric takes "" as a number)
			{
				Numbers[std::string(TCHAR_TO_UTF8(*Key))] = FCString::Atod(*Val);
				UE_LOG(LogTheater, Display, TEXT("Battle effects: the recipe's %s set to %s from the launch line"), *Key, *Val);
			}
			else
			{
				UE_LOG(LogTheater, Warning, TEXT("Battle effects: -TheaterFxSet part \"%s\" is not path=number; left out"), *Part);
			}
		}
	}
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

// (nothing of the engine's is touched here: it may be run while the world is being torn down; End does that. Only the
// kit's own Draw, if it is still on its worker, is waited for: it uses this)
FBattleEffects::~FBattleEffects()
{
	if (bDrawing && Drawing.IsValid())
	{
		Drawing.Wait();
	}
}

template <typename FWork>
void FBattleEffects::Do(FWork&& Work, bool bWhenReady)
{
	// (the production lead's FX2: while the kit draws on its worker, TickBegin to TickEnd, what is asked of it waits
	// here, in the order asked, with its arguments copied, and TickEnd does it. Otherwise it is done now, as ever)
	if (bWhenReady && !IsReady())
	{
		return;
	}
	if (bDrawing)
	{
		Held.Emplace(Forward<FWork>(Work));
		return;
	}
	Work();
}

void FBattleEffects::End()
{
	// (the kit drawing on its worker: waited for, and what was asked of it meanwhile let go with the rest)
	if (bDrawing && Drawing.IsValid())
	{
		Drawing.Wait();
	}
	Drawing = UE::Tasks::TTask<int32>();
	bDrawing = false;
	Held.Reset();
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
	bSoftInk = false;
	Holder.Reset();
	AskedRule = TheaterFx::Battle::LampRule(); // (a kit begun again for another scene hands its lamps out as ever until that one asks)
	Core.Clear();
	TestShots.Reset();
	TestStart = -1.0;
	Sections = 0;
	SectionsUsed = 0;
	bShown = false;
	LogMost = -1;
	LogFrom = -1.0;
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

bool FBattleEffects::GroundColourAt(void* Context, double X, double Y, double& Red, double& Green, double& Blue)
{
	const FBattleEffects* Self = static_cast<const FBattleEffects*>(Context);
	if (!Self || !Self->GroundColour)
	{
		return false;
	}
	const double U = Self->UnitsPerMetre;
	FLinearColor Seen(0.0f, 0.0f, 0.0f, 0.0f);
	if (!Self->GroundColour(FVector(X * U, Y * U, 0.0), Seen))
	{
		return false;
	}
	Red = 255.0 * Seen.R;
	Green = 255.0 * Seen.G;
	Blue = 255.0 * Seen.B;
	return true;
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
	TArray<FString> OverPaths;
	for (const FString& Name : Over)
	{
		OverPaths.Add(FPaths::ProjectContentDir() / TEXT("Effects") / Name);
	}
	if (!Book.Load(Path, OverPaths))
	{
		UE_LOG(LogTheater, Warning, TEXT("Battle effects: the recipe is missing or unreadable (%s); nothing will be drawn"), *Path);
		return false;
	}
	// (M_BattlePuffs reads the colours as sRGB bytes, so a dark smoke keeps its hue; without it (build_content.py not run
	// since it came), M_Highlight and plain bytes, as before)
	UMaterialInterface* Puffs = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Campaign/Materials/M_BattlePuffs.M_BattlePuffs"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	const bool bGammaBytes = Puffs != nullptr;
	bOwnPuffs = bGammaBytes;
	UMaterialInterface* Highlight = Puffs ? Puffs : LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Campaign/Materials/M_Highlight.M_Highlight"));
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
	Core.SetGroundColour(&FBattleEffects::GroundColourAt, this);
	Core.SetSrgbBytes(bGammaBytes); // (before any Draw, on this thread: no task is running yet)
	// (picture.lit_gain: the kit writes its colours that much darker and its hottest lights up to that much past white,
	// so the material's brightness must be the same number for all else to look as it did)
	InkBrightness = static_cast<float>(Core.PictureGain());
	Instance->SetScalarParameterValue(TEXT("Brightness"), InkBrightness);
	bSoftInk = false;
	double Candela = 2000.0;
	Book.Find("game.lamp_candela_at_full", Candela);
	LampsInUse = LampsAsked(); // (game.max_lamps, or the scene's SetLampRule given before Begin)
	LampCandela = Candela * FMath::Square(UnitsPerMetre / 100.0); // (the recipe's candelas are at 100 units a metre)
	for (int32 Index = 0; Index < LampsInUse; ++Index)
	{
		LampLights[Index] = MakeLamp(Owner, Parent);
	}
	Core.SetLampRule(AskedRule); // (Setup put the kit's back to the default; the scene's, given before Begin, again)
	if (AskedRule.Asks())
	{
		LampsByRule();
	}
	const UWorld* World = Parent->GetWorld();
	Clock = World ? World->GetTimeSeconds() : 0.0; // (until the first Tick)
	UE_LOG(LogTheater, Log, TEXT("Battle effects ready: %.2f world units a metre, room for %d puffs, %d lamps; colours %s"), UnitsPerMetre, Core.Room(), LampsInUse,
		bGammaBytes ? TEXT("as sRGB bytes (M_BattlePuffs)") : TEXT("as plain bytes (M_Highlight: run Tools/build_content.sh for M_BattlePuffs, darker smoke keeps its hue)"));
	return true;
}

bool FBattleEffects::IsReady() const
{
	return Mesh.IsValid() && Ink.IsValid();
}

UPointLightComponent* FBattleEffects::MakeLamp(AActor* Owner, USceneComponent* Parent)
{
	// (a flash's lamp: shadowless, out until a flash lights one)
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
	return Lamp;
}

void FBattleEffects::LampsByRule()
{
	// (the look review of 25 November: under a scene's rule (the sea's) a lamp is a gun flash of 6,600 to 58,000 cd
	// reaching 115 to 360 m and lit for 6 to 12 frames. Lumen's surface cache is lit over several frames and cannot
	// follow it: it would cost GPU time and leave an after-glow on hulls and water. So such lamps light directly
	// only, and put nothing into the volumetric fog. Lamps made without a rule (ground, air) are never touched: the
	// engine's defaults, 1 and 1, as ever; a rule taken back to the default puts them back)
	const float Indirect = AskedRule.Asks() ? 0.0f : 1.0f;
	for (int32 Index = 0; Index < LampsInUse; ++Index)
	{
		if (LampLights[Index].IsValid())
		{
			LampLights[Index]->SetIndirectLightingIntensity(Indirect);
			LampLights[Index]->SetVolumetricScatteringIntensity(Indirect);
		}
	}
}

int32 FBattleEffects::LampsAsked() const
{
	// (the scene's rule's Most, or game.max_lamps (2); never over MostLamps, the kit's own cap)
	if (AskedRule.Most > 0)
	{
		return FMath::Clamp(AskedRule.Most, 0, MostLamps);
	}
	double Wanted = 2.0;
	Book.Find("game.max_lamps", Wanted);
	return FMath::Clamp(static_cast<int32>(Wanted), 0, MostLamps);
}

void FBattleEffects::SetLampRule(const TheaterFx::Battle::LampRule& Rule)
{
	// (the sea's night fight, 25 November: see the header. Before Begin it is kept for Begin, which makes that many lamps;
	// after it the lamps still wanted are made here, on this thread, and any no longer wanted put out. A scene that never
	// calls it makes no lamp more than game.max_lamps, and its lamps go as ever)
	AskedRule = Rule;
	AskedRule.Most = FMath::Clamp(Rule.Most, 0, MostLamps);
	if (IsReady() && Holder.IsValid() && Holder->GetOwner())
	{
		const int32 Want = LampsAsked();
		for (int32 Index = 0; Index < Want; ++Index)
		{
			if (!LampLights[Index].IsValid())
			{
				LampLights[Index] = MakeLamp(Holder->GetOwner(), Holder.Get());
			}
		}
		for (int32 Index = Want; Index < LampsInUse; ++Index)
		{
			if (LampLights[Index].IsValid())
			{
				LampLights[Index]->SetVisibility(false);
			}
		}
		LampsInUse = Want;
		LampsByRule();
		UE_LOG(LogTheater, Log, TEXT("Battle effects: lamp rule %d lamps, by the eye %s, a salvo within %.0f m and %.2f s one lamp at most %.1f times its strongest"), LampsInUse, AskedRule.bByEye ? TEXT("on") : TEXT("off"), AskedRule.MergeM, AskedRule.MergeS, AskedRule.MergeMost);
	}
	const TheaterFx::Battle::LampRule Given = AskedRule;
	Do([=, this]()
	{
		Core.SetLampRule(Given);
	}, false);
}

int32 FBattleEffects::PuffsDrawn() const
{
	return LastDrawn; // (as at the last Tick or TickEnd: never read from the kit while it may be drawing)
}

int32 FBattleEffects::PuffsAsked() const
{
	return LastAsked;
}

std::string FBattleEffects::Named(const FString& Text)
{
	// (the recipe's rows are lower case and its look-ups exact: "Earth" or "HMG" from a caller finds its row)
	return std::string(TCHAR_TO_UTF8(*Text.ToLower()));
}

void FBattleEffects::Shell(const FVector& WorldAt, const FString& Calibre, const FString& Ground, bool bOnArmour)
{
	Do([=, this]()
	{
		const std::string Size = Core.SizeOfCalibre(Named(Calibre).c_str());
		Core.Shell(Time(), ToFx(WorldAt), Size.c_str(), Named(Ground).c_str(), bOnArmour);
	});
}

void FBattleEffects::ShellOfSize(const FVector& WorldAt, const FString& Size, const FString& Ground, bool bOnArmour)
{
	Do([=, this]()
	{
		Core.Shell(Time(), ToFx(WorldAt), Named(Size).c_str(), Named(Ground).c_str(), bOnArmour);
	});
}

void FBattleEffects::Skip(const FVector& WorldAt, const FVector& Heading, const FString& Ground)
{
	Do([=, this]()
	{
		Core.Skip(Time(), ToFx(WorldAt), ToFxWay(Heading), Named(Ground).c_str());
	});
}

void FBattleEffects::Muzzle(const FVector& WorldAt, const FVector& Facing, const FString& Gun, const FString& Ground)
{
	Do([=, this]()
	{
		Core.Muzzle(Time(), ToFx(WorldAt), ToFxWay(Facing), Named(Gun).c_str(), Named(Ground).c_str());
	});
}

void FBattleEffects::VehicleDust(int32 Id, const FVector& SocketAt, const FVector& Velocity, const FString& Ground, bool bTracked)
{
	Do([=, this]()
	{
		Core.VehicleDust(Time(), Id, ToFx(SocketAt), ToFx(Velocity), Named(Ground).c_str(), bTracked);
	});
}

void FBattleEffects::Shots(int32 Gun, const FVector& From, const FVector& To, const FString& Weapon, int32 Rounds, float PerSecond, const FString& Ground)
{
	Do([=, this]()
	{
		Core.Shots(Time(), Gun, ToFx(From), ToFx(To), Named(Weapon).c_str(), Rounds, PerSecond, Named(Ground).c_str());
	});
}

void FBattleEffects::ShotsOnArmour(int32 Gun, const FVector& From, const FVector& To, const FVector& Normal, const FVector& RoomMid, const FVector& RoomA, const FVector& RoomB, const FVector& Carry, const FVector& Pivot, double Spin, double FloorZ,
	const TArray<FVector>& PathAt, const TArray<double>& PathDt, const TArray<double>& PathTurn, const FString& Weapon, int32 Rounds, float PerSecond, const FString& Ground)
{
	Do([=, this]()
	{
		TheaterFx::Battle::ArmourPlate Plate;
		Plate.At = ToFx(To);
		Plate.Normal = ToFxWay(Normal);
		Plate.RoomMid = ToFx(RoomMid);
		Plate.RoomA = ToFx(RoomA);
		Plate.RoomB = ToFx(RoomB);
		Plate.Carry = ToFx(Carry);
		Plate.Pivot = ToFx(Pivot);
		Plate.Spin = Spin;
		Plate.Floor = FloorZ > -1.0e29 ? FloorZ / UnitsPerMetre : -1.0e30;
		// (her path, where the three lists agree: the kit checks the rest)
		if (PathAt.Num() >= 2 && PathAt.Num() == PathDt.Num() && PathAt.Num() == PathTurn.Num())
		{
			Plate.Path.reserve(static_cast<size_t>(PathAt.Num()));
			for (int32 K = 0; K < PathAt.Num(); ++K)
			{
				TheaterFx::Battle::ArmourPlate::PathPoint Pt;
				Pt.Dt = PathDt[K];
				Pt.At = ToFx(PathAt[K]);
				Pt.Turn = PathTurn[K];
				Plate.Path.push_back(Pt);
			}
		}
		Core.ShotsOnArmour(Time(), Gun, ToFx(From), Plate, Named(Weapon).c_str(), Rounds, PerSecond, Named(Ground).c_str());
	});
}

void FBattleEffects::AirGun(double Now, int32 Gun, const FVector& Muzzle, const FVector& Aim, const FVector& ShooterVelocity, const FString& Kind, float RatePerMinute, float MuzzleSpeed, const FString& Nation)
{
	Do([=, this]()
	{
		// (velocities are world units a second, as places are: the same ToFx turns both to metres)
		Core.AirGun(Now, Gun, ToFx(Muzzle), ToFx(Aim), ToFx(ShooterVelocity), Named(Kind).c_str(), RatePerMinute, MuzzleSpeed / UnitsPerMetre, Named(Nation).c_str());
	});
}

void FBattleEffects::AirGunStop(int32 Gun)
{
	Do([=, this]()
	{
		Core.AirGunStop(Gun);
	});
}

void FBattleEffects::AirHit(double Now, const FVector& At, const FVector& Normal, const FVector& AircraftVelocity, const FString& Kind)
{
	Do([=, this]()
	{
		Core.AirHit(Now, ToFx(At), ToFxWay(Normal), ToFx(AircraftVelocity), Named(Kind).c_str());
	});
}

void FBattleEffects::TrailStart(double Now, int32 Id, const FVector& At, const FVector& Velocity, const FString& Kind, float SpanMetres, int32 Look)
{
	Do([=, this]()
	{
		Core.TrailStart(Now, Id, ToFx(At), ToFx(Velocity), Named(Kind).c_str(), SpanMetres, Look);
	});
}

void FBattleEffects::TrailMove(double Now, int32 Id, const FVector& At, const FVector& Velocity, float Power)
{
	Do([=, this]()
	{
		Core.TrailMove(Now, Id, ToFx(At), ToFx(Velocity), Power);
	});
}

void FBattleEffects::TrailStop(int32 Id)
{
	Do([=, this]()
	{
		Core.TrailStop(Id);
	});
}

void FBattleEffects::AirFall(double Now, int32 Id, const FVector& At, const FVector& Velocity, const FString& Kind, float SpanMetres, double DeckZ, int32 Look, bool bBombs)
{
	Do([=, this]()
	{
		Core.AirFall(Now, Id, ToFx(At), ToFx(Velocity), Named(Kind).c_str(), SpanMetres, DeckZ / UnitsPerMetre, Look, bBombs);
	});
}

void FBattleEffects::AirFallMove(double Now, int32 Id, const FVector& At, const FVector& Velocity)
{
	Do([=, this]()
	{
		Core.AirFallMove(Now, Id, ToFx(At), ToFx(Velocity));
	});
}

void FBattleEffects::AirFallStop(int32 Id)
{
	Do([=, this]()
	{
		Core.AirFallStop(Id);
	});
}

void FBattleEffects::AirFlak(double Now, const FVector& At, const FString& Kind)
{
	Do([=, this]()
	{
		Core.AirFlak(Now, ToFx(At), Named(Kind).c_str());
	});
}

void FBattleEffects::BombStick(double Now, int32 Id, const FVector& First, const FVector& Last, int32 Count, const FString& Size, const FString& Ground, float OverSeconds)
{
	Do([=, this]()
	{
		Core.BombStick(Now, Id, ToFx(First), ToFx(Last), Count, Named(Size).c_str(), Named(Ground).c_str(), OverSeconds);
	});
}

void FBattleEffects::Burn(const FVector& WorldAt, float AcrossMetres, float Seconds, int32 Id, bool bOnShip)
{
	Do([=, this]()
	{
		Core.Burn(Time(), ToFx(WorldAt), AcrossMetres, Seconds, Id, bOnShip);
	});
}

void FBattleEffects::Vent(const FVector& WorldAt, float AcrossMetres, float Seconds, int32 Id)
{
	Do([=, this]()
	{
		Core.Vent(Time(), ToFx(WorldAt), AcrossMetres, Seconds, Id);
	});
}

void FBattleEffects::FuelBall(const FVector& WorldAt, float AcrossMetres, const FVector& Velocity)
{
	Do([=, this]()
	{
		Core.FuelBall(Time(), ToFx(WorldAt), AcrossMetres, ToFx(Velocity));
	});
}

void FBattleEffects::BurnOut(int32 Id, float OverSeconds, const FString& How)
{
	Do([=, this]()
	{
		Core.BurnOut(Time(), Id, OverSeconds, Named(How).c_str());
	});
}

void FBattleEffects::BurnMove(int32 Id, const FVector& WorldAt, const FVector& Velocity)
{
	Do([=, this]()
	{
		Core.BurnMove(Time(), Id, ToFx(WorldAt), ToFx(Velocity));
	});
}

void FBattleEffects::Steam(int32 Id, const FVector& WorldAt, const FVector& Way, float AcrossMetres, float Seconds, const FString& Kind)
{
	Do([=, this]()
	{
		Core.Steam(Time(), Id, ToFx(WorldAt), ToFxWay(Way), AcrossMetres, Seconds, Named(Kind).c_str());
	});
}

void FBattleEffects::SteamStop(int32 Id)
{
	Do([=, this]()
	{
		Core.SteamStop(Time(), Id);
	});
}

void FBattleEffects::SteamMove(int32 Id, const FVector& WorldAt, const FVector& Way, const FVector& Velocity)
{
	Do([=, this]()
	{
		Core.SteamMove(Time(), Id, ToFx(WorldAt), ToFxWay(Way), ToFx(Velocity));
	});
}

void FBattleEffects::CookOff(const FVector& WorldAt, const FVector& Up, float AcrossMetres, int32 Pops, float OverSeconds, float JetSeconds, float JetFromSeconds, int32 Id)
{
	Do([=, this]()
	{
		Core.CookOff(Time(), ToFx(WorldAt), ToFxWay(Up), AcrossMetres, Pops, OverSeconds, JetSeconds, JetFromSeconds, Id);
	});
}

void FBattleEffects::CookOffMove(int32 Id, const FVector& WorldAt, const FVector& Up, const FVector& Velocity)
{
	Do([=, this]()
	{
		Core.CookOffMove(Time(), Id, ToFx(WorldAt), ToFxWay(Up), ToFx(Velocity));
	});
}

void FBattleEffects::Glance(const FVector& WorldAt, const FVector& Incoming, const FVector& Normal, float CalibreMm, const FString& Ground)
{
	Do([=, this]()
	{
		Core.Glance(Time(), ToFx(WorldAt), ToFxWay(Incoming), ToFxWay(Normal), CalibreMm, Named(Ground).c_str());
	});
}

void FBattleEffects::OilFire(int32 Id, const FVector& WorldAt, float ReachMetres, const FString& Kind, double UntilSeconds, float SinceSeconds)
{
	Do([=, this]()
	{
		Core.OilFire(Time(), Id, ToFx(WorldAt), ReachMetres, Named(Kind).c_str(), UntilSeconds, SinceSeconds);
	});
}

void FBattleEffects::OilStop(int32 Id)
{
	Do([=, this]()
	{
		Core.OilStop(Time(), Id);
	});
}

void FBattleEffects::SmokeScreen(const FVector& WorldAt, float AcrossMetres, float Seconds, const FString& Kind, int32 Id)
{
	Do([=, this]()
	{
		Core.Screen(Time(), ToFx(WorldAt), AcrossMetres, Seconds, Named(Kind).c_str(), Id);
	});
}

void FBattleEffects::ScreenLay(int32 Id, const FVector& WorldAt, const FVector& Velocity, bool bFunnel)
{
	Do([=, this]()
	{
		Core.ScreenLay(Time(), Id, ToFx(WorldAt), ToFx(Velocity), bFunnel);
	});
}

void FBattleEffects::ScreenLayStop(int32 Id)
{
	Do([=, this]()
	{
		Core.ScreenLayStop(Time(), Id);
	});
}

void FBattleEffects::SeaSplash(const FVector& WorldAt, float CalibreMm, const FString& Dye)
{
	Do([=, this]()
	{
		Core.SeaSplash(Time(), ToFx(WorldAt), CalibreMm, Named(Dye).c_str());
	});
}

void FBattleEffects::SetWind(const FVector& MetresPerSecond)
{
	Do([=, this]()
	{
		Core.SetWind(ToFxWay(MetresPerSecond));
	}, false);
}

void FBattleEffects::SetSky(const FLinearColor& OnScreen)
{
	Do([=, this]()
	{
		Core.SetSky(255.0 * OnScreen.R, 255.0 * OnScreen.G, 255.0 * OnScreen.B);
	}, false);
}

void FBattleEffects::UseAirDistance()
{
	Do([=, this]()
	{
		Core.SetDrawBeyond(Core.AirBeyond());
	});
}

void FBattleEffects::UseSeaDistance()
{
	Do([=, this]()
	{
		Core.UseSea();
	});
}

void FBattleEffects::SetLensFade(float GoneMetres, float WholeMetres, float PlaneMetres)
{
	Do([=, this]()
	{
		Core.SetLensFade(GoneMetres, WholeMetres, PlaneMetres);
	});
}

void FBattleEffects::SetCraters(bool bOn)
{
	Do([=, this]()
	{
		Core.SetScars(bOn);
	});
}

void FBattleEffects::SetLandGrade(bool bOn)
{
	Do([=, this]()
	{
		Core.SetLandGrade(bOn);
	});
}

void FBattleEffects::SetFromAbove(bool bOn)
{
	Do([=, this]()
	{
		Core.SetFromAbove(bOn);
	});
}

void FBattleEffects::SetSun(const FVector& TowardSun)
{
	Sun = TowardSun.GetSafeNormal(UE_SMALL_NUMBER, FVector(-0.45, -0.35, 0.82));
}

bool FBattleEffects::SetFireSpread(FPostProcessSettings& Lens, float Spread, float Warm)
{
	// (on this thread only: the lens and the mesh's custom depth are the scene's, and the kit's worker reads neither)
	if (UMaterialInstanceDynamic* Before = SpreadInk.Get())
	{
		Lens.WeightedBlendables.Array.RemoveAll([Before](const FWeightedBlendable& Each) { return Each.Object == Before; });
	}
	SpreadInk.Reset();
	if (!Mesh.IsValid())
	{
		UE_LOG(LogTheater, Warning, TEXT("Battle effects: the fire's spread asked before Begin; none"));
		return false;
	}
	if (!(Spread > 1.0f))
	{
		Mesh->SetRenderCustomDepth(false);
		return true;
	}
	UMaterialInterface* Spreader = bOwnPuffs ? LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Campaign/Materials/M_FireSpread.M_FireSpread"), nullptr, LOAD_NoWarn | LOAD_Quiet) : nullptr;
	if (!Spreader)
	{
		Mesh->SetRenderCustomDepth(false);
		UE_LOG(LogTheater, Warning, TEXT("Battle effects: %s is missing, so fire takes the scene's grade as it is (run Tools/build_content.sh with TC_REBUILD=M_BattlePuffs,M_FireSpread)"),
			bOwnPuffs ? TEXT("M_FireSpread") : TEXT("M_BattlePuffs"));
		return false;
	}
	UMaterialInstanceDynamic* Instance = UMaterialInstanceDynamic::Create(Spreader, Mesh.Get());
	Instance->SetScalarParameterValue(TEXT("Spread"), Spread);
	Instance->SetScalarParameterValue(TEXT("Warm"), FMath::Max(Warm, 0.0f));
	Lens.WeightedBlendables.Array.Add(FWeightedBlendable(1.0f, Instance));
	SpreadInk = Instance;
	Mesh->SetRenderCustomDepth(true);
	UE_LOG(LogTheater, Log, TEXT("Battle effects: the fire's spread after the grade, %.2f, green %.2f"), Spread, Warm);
	return true;
}

bool FBattleEffects::SetSoftContact(float Metres)
{
	// (on this thread only: the mesh's materials are the scene's, and the kit's worker lays only places and colours)
	if (!Mesh.IsValid() || !Ink.IsValid())
	{
		UE_LOG(LogTheater, Warning, TEXT("Battle effects: soft contact asked before Begin; none"));
		return false;
	}
	const bool bWanted = Metres > 0.0f && Metres < 100.0f;
	UMaterialInterface* Soft = bWanted && bOwnPuffs ? LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Campaign/Materials/M_BattlePuffsSoft.M_BattlePuffsSoft"), nullptr, LOAD_NoWarn | LOAD_Quiet) : nullptr;
	if (bWanted && !Soft)
	{
		UE_LOG(LogTheater, Warning, TEXT("Battle effects: %s is missing, so the kit's pieces meet the ground and hulls as before (run Tools/build_content.sh with TC_REBUILD=M_BattlePuffsSoft)"),
			bOwnPuffs ? TEXT("M_BattlePuffsSoft") : TEXT("M_BattlePuffs"));
	}
	const bool bSoft = Soft != nullptr;
	if (bSoft != bSoftInk)
	{
		// (a new instance of the other material, as bright as Begin made the first, on every section the mesh has)
		UMaterialInterface* Base = bSoft ? Soft : LoadObject<UMaterialInterface>(nullptr, bOwnPuffs ? TEXT("/Game/Campaign/Materials/M_BattlePuffs.M_BattlePuffs") : TEXT("/Game/Campaign/Materials/M_Highlight.M_Highlight"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!Base)
		{
			return false; // (the plain material gone since Begin: Ink stays as it is)
		}
		UMaterialInstanceDynamic* Instance = UMaterialInstanceDynamic::Create(Base, Mesh.Get());
		Instance->SetScalarParameterValue(TEXT("Brightness"), InkBrightness);
		for (int32 Section = 0; Section < FMath::Max(Sections, 1); ++Section)
		{
			Mesh->SetMaterial(Section, Instance);
		}
		Ink = Instance;
		bSoftInk = bSoft;
	}
	if (bSoft)
	{
		Ink->SetScalarParameterValue(TEXT("FadeCm"), static_cast<float>(Metres * UnitsPerMetre)); // (the material's depths are in world units)
	}
	const double Lying = bSoft ? static_cast<double>(Metres) : 0.0;
	Do([=, this]()
	{
		Core.SetSoftContact(Lying);
	});
	if (bSoft)
	{
		UE_LOG(LogTheater, Log, TEXT("Battle effects: soft contact, %.2f m"), Metres);
	}
	return bSoft || !bWanted;
}

void FBattleEffects::Clear()
{
	TickEnd(); // (the kit drawing on its worker: it ends that frame first)
	Core.Clear();
	TestShots.Reset();
	TestStart = -1.0;
	Hide();
}

void FBattleEffects::MakeSections(const FVector& Fill)
{
	// The mesh, made once (the production lead's FX1, 22 November): the kit's room in sections of PuffsASection puffs
	// (the map smoke's shape: a middle and two rings, of game.puff_sides points each, both faces of every triangle).
	// Before, one section grew and shrank with the puffs in steps of 300, and each step rebuilt the whole of it, up to
	// 75,002 points and 648k indices, 16 to 22 times a minute on the Mac. Now only points and colours ever change, and
	// only in the sections that hold puffs. The kit lays its puffs far to near and section k holds its puffs from
	// k x PuffsASection on, so the sections, drawn in their order, keep the far-to-near order the translucent puffs need.
	const int32 Ring = Core.Ring(), PerPuff = Core.PerPuff();
	TArray<int32> Triangles;
	Triangles.Reserve(PuffsASection * Ring * 18);
	for (int32 Each = 0; Each < PuffsASection; ++Each)
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
	Points.Init(Fill, PuffsASection * PerPuff); // (folded at the battle, not at the world's origin, so the bounds stay tight)
	Colours.Init(FColor(0, 0, 0, 0), PuffsASection * PerPuff);
	TArray<FVector> Normals;
	Normals.Init(FVector::UpVector, Points.Num());
	Sections = FMath::Max(1, FMath::DivideAndRoundUp(Core.Room(), PuffsASection));
	for (int32 Section = 0; Section < Sections; ++Section)
	{
		Mesh->CreateMeshSection(Section, Points, Triangles, Normals, TArray<FVector2D>(), Colours, TArray<FProcMeshTangent>(), false);
		Mesh->SetMaterial(Section, Ink.Get());
		Mesh->SetMeshSectionVisible(Section, false);
	}
	SectionsUsed = 0;
	FoldedAt = Fill;
}

void FBattleEffects::RoomLog(double Now, int32 Laid)
{
	// (-TheaterFxRoomLog: every 10 s of the kit's own clock, Tick's Now, the most puffs drawn in a frame in those 10 s,
	// when, and the room asked for at that frame. Over 3,000 asked: the room was short and the kit chose what to leave out)
	static const bool bRoomLog = FParse::Param(FCommandLine::Get(), TEXT("TheaterFxRoomLog"));
	if (!bRoomLog)
	{
		return;
	}
	if (LogFrom < 0.0 || Now < LogFrom)
	{
		LogFrom = Now;
		LogMost = -1;
	}
	if (Laid > LogMost)
	{
		LogMost = Laid;
		LogMostAt = Now;
		LogAsked = Core.PuffsAsked();
	}
	if (Now - LogFrom >= 10.0)
	{
		UE_LOG(LogTheater, Log, TEXT("Battle effects room, the kit's clock %.1f to %.1f s: the most drawn %d at %.1f s, asked %d then, of room %d; %d of %d sections in use now; land hits so far %d, %d with fewer pieces (burst.far), keeping %.2f of their pieces on average"), LogFrom, Now, LogMost, LogMostAt, LogAsked, Core.Room(), SectionsUsed, Sections, Core.LandHitsMade(), Core.LandHitsThinned(), Core.LandKeptMean());
		LogFrom = Now;
		LogMost = -1;
	}
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

FBattleJolt FBattleEffects::Tick(double Now, const FVector& Eye, const FRotator& Facing, float FieldOfViewDegrees, int32 ViewWidth, const FLinearColor& HourLight, int32 ViewHeight)
{
	// (all on this thread, as before FX2: the air and sea scenes' way)
	TickEnd();
	FBattleJolt Jolt;
	TheaterFx::Battle::View Sight;
	double Day = 1.0;
	if (Ahead(Now, Eye, Facing, FieldOfViewDegrees, ViewWidth, HourLight, ViewHeight, Jolt, Sight, Day))
	{
		Lay(Now, Eye, Core.Draw(Now, Sight), Day);
	}
	return Jolt;
}

FBattleJolt FBattleEffects::TickBegin(double Now, const FVector& Eye, const FRotator& Facing, float FieldOfViewDegrees, int32 ViewWidth, const FLinearColor& HourLight, int32 ViewHeight)
{
	TickEnd(); // (a TickBegin never ended: ended now)
	FBattleJolt Jolt;
	TheaterFx::Battle::View Sight;
	double Day = 1.0;
	if (Ahead(Now, Eye, Facing, FieldOfViewDegrees, ViewWidth, HourLight, ViewHeight, Jolt, Sight, Day))
	{
		// (the production lead's FX2: the kit's own frame, all it does in Draw (its sources run, the old pieces go, the
		// living ones laid far to near into its points and colours), on a worker while the scene draws its men and
		// tanks; TickEnd sends what it laid. Nothing else touches the kit until then: calls are held, Do)
		DrawNow = Now;
		DrawEye = Eye;
		DrawDay = Day;
		bDrawing = true;
		Drawing = UE::Tasks::Launch(TEXT("Battle effects: the kit's Draw"), [this, Now, Sight]() { return static_cast<int32>(Core.Draw(Now, Sight)); });
	}
	return Jolt;
}

void FBattleEffects::TickEnd()
{
	if (!bDrawing)
	{
		return;
	}
	const int32 Drawn = Drawing.GetResult(); // (waits for the worker, if it is still at it)
	Drawing = UE::Tasks::TTask<int32>();
	bDrawing = false;
	Lay(DrawNow, DrawEye, Drawn, DrawDay);
	// (what the scene asked of the kit while it drew, in the order asked: each stamped as it was asked, with this
	// frame's time, so it is first seen at the next frame, as a call made after a Tick is)
	TArray<TUniqueFunction<void()>> Asked = MoveTemp(Held);
	Held.Reset();
	for (TUniqueFunction<void()>& Work : Asked)
	{
		Work();
	}
}

bool FBattleEffects::Ahead(double Now, const FVector& Eye, const FRotator& Facing, float FieldOfViewDegrees, int32 ViewWidth, const FLinearColor& HourLight, int32 ViewHeight, FBattleJolt& Jolt, TheaterFx::Battle::View& Sight, double& Day)
{
	// (what must be done on this thread before the kit draws: the test barrage, the camera's jolt, the view)
	if (!IsReady())
	{
		return false;
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
		LastDrawn = 0;
		LastAsked = Core.PuffsAsked();
		return false;
	}

	const FRotationMatrix Looking(Facing);
	Sight = TheaterFx::Battle::View();
	Sight.Eye = EyeAt;
	Sight.Forward = ToFxWay(Looking.GetUnitAxis(EAxis::X));
	Sight.Right = ToFxWay(Looking.GetUnitAxis(EAxis::Y));
	Sight.Up = ToFxWay(Looking.GetUnitAxis(EAxis::Z));
	const double HalfView = FMath::DegreesToRadians(FMath::Clamp(static_cast<double>(FieldOfViewDegrees), 5.0, 170.0) * 0.5);
	Sight.PixelsPerRadian = FMath::Max(1, ViewWidth) / (2.0 * FMath::Tan(HalfView));
	Sight.HalfAcross = FMath::Tan(HalfView); // (what is wholly off the screen takes no place; up and down a quarter more, when the height is not given)
	if (ViewHeight > 0 && ViewWidth > 0)
	{
		Sight.HalfUp = Sight.HalfAcross * static_cast<double>(ViewHeight) / static_cast<double>(ViewWidth); // (a window taller than wide lost pieces at its top and bottom. The pre-check, 15:02Z)
	}
	Sight.SmokeLight = TheaterFx::Rgb(HourLight.R, HourLight.G, HourLight.B);
	Sight.Sun = ToFxWay(Sun);
	const float SunUp = static_cast<float>(Core.DayOf(Sight.SmokeLight)); // (the sun's own light: full by day, none at night)
	Sight.SunLight = TheaterFx::Rgb(SunUp, SunUp, SunUp);
	Day = Core.DayOf(Sight.SmokeLight);
	return true;
}

void FBattleEffects::Lay(double Now, const FVector& Eye, int32 Drawn, double Day)
{
	// (after the kit's Draw, on this thread: its puffs into the mesh, and the lamps)
	if (!IsReady())
	{
		return;
	}
	const int32 PerPuff = Core.PerPuff();
	const std::vector<TheaterFx::Vec>& From = Core.Points();
	const std::vector<uint8_t>& Rgba = Core.Colours();
	const FVector FirstAt = Drawn > 0 ? FVector(From[0].X, From[0].Y, From[0].Z) * UnitsPerMetre : Eye;
	if (Sections == 0)
	{
		MakeSections(FirstAt);
	}
	// (the sections that hold puffs now are sent, their slots past the last puff folded flat onto their first point; a
	// section that held puffs at the last Tick and holds none now is sent once more, all of it folded onto the first
	// puff's first point (so the mesh's bounds stay round the battle), and hidden; the rest are not touched)
	const int32 Used = FMath::Min(Sections, FMath::DivideAndRoundUp(FMath::Max(Drawn, 0), PuffsASection));
	const int32 Slots = PuffsASection * PerPuff;
	for (int32 Section = 0; Section < FMath::Max(Used, SectionsUsed); ++Section)
	{
		const int32 FirstPuff = Section * PuffsASection;
		const int32 Real = FMath::Clamp(Drawn - FirstPuff, 0, PuffsASection) * PerPuff;
		for (int32 Index = 0; Index < Real; ++Index)
		{
			const size_t At = static_cast<size_t>(FirstPuff) * PerPuff + Index;
			const TheaterFx::Vec& P = From[At];
			Points[Index] = FVector(P.X, P.Y, P.Z) * UnitsPerMetre;
			const size_t C = At * 4;
			Colours[Index] = FColor(Rgba[C], Rgba[C + 1], Rgba[C + 2], Rgba[C + 3]); // (linear already, as ToFColor(false))
		}
		const FVector Fold = Real > 0 ? Points[0] : FirstAt;
		for (int32 Index = Real; Index < Slots; ++Index)
		{
			Points[Index] = Fold;
			Colours[Index] = FColor(0, 0, 0, 0);
		}
		Mesh->UpdateMeshSection(Section, Points, TArray<FVector>(), TArray<FVector2D>(), Colours, TArray<FProcMeshTangent>());
		const bool bUse = Section < Used;
		if (bUse != (Section < SectionsUsed))
		{
			Mesh->SetMeshSectionVisible(Section, bUse);
		}
	}
	// (the pre-check on FX1: a hidden section's box still counts in the mesh's bounds, and the translucent sort goes by
	// the bounds' middle, so sections folded where the battle was stretch the box as the battle moves on, a bomber
	// stream most. When the first puff is more than 500 m from where they were folded, every section not sent above is
	// sent once more, folded onto it and still hidden: the box stays within 500 m of the puffs, at a cost of one send
	// of the idle sections each 500 m the battle moves)
	const int32 Sent = FMath::Max(Used, SectionsUsed);
	if (Sent >= Sections)
	{
		FoldedAt = FirstAt; // (every section was sent at the battle as it is now)
	}
	else if (FVector::DistSquared(FirstAt, FoldedAt) > FMath::Square(500.0 * UnitsPerMetre))
	{
		for (int32 Index = 0; Index < Slots; ++Index)
		{
			Points[Index] = FirstAt;
			Colours[Index] = FColor(0, 0, 0, 0);
		}
		for (int32 Section = Sent; Section < Sections; ++Section)
		{
			Mesh->UpdateMeshSection(Section, Points, TArray<FVector>(), TArray<FVector2D>(), Colours, TArray<FProcMeshTangent>());
		}
		FoldedAt = FirstAt;
	}
	SectionsUsed = Used;
	LastDrawn = Drawn;
	LastAsked = Core.PuffsAsked();
	RoomLog(Now, Drawn);
	if (!bShown)
	{
		Mesh->SetVisibility(true);
		bShown = true;
	}

	// the lamps: the strongest flashes now, out the moment they are spent
	TheaterFx::Battle::Lamp Lit[MostLamps];
	const int32 NLit = Core.Lamps(Now, Lit, LampsInUse, Day); // (after Draw: a flash's lamp goes by the frames it is drawn in)
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
}

bool FBattleEffects::WantsTest()
{
	return FParse::Param(FCommandLine::Get(), TEXT("TheaterBlastTest"));
}

void FBattleEffects::RunTest(const FVector& Centre, const FString& GroundKind)
{
	// The test barrage, laid out in metres round Centre and played again every 24 s: each calibre on earth in a row
	// across the view, then sand, snow and mud, a bomb further off, a hit on armour, a skip, guns firing tracer and a
	// wreck burning. GroundKind: the kit's ground under Centre, which the tank gun's and the machine gun's dust take
	// (art director 18:28Z: dust takes the colour of the ground it rose from); the shells keep their own grounds.
	TestCentre = Centre;
	TestGround = GroundKind.IsEmpty() ? FString(TEXT("earth")) : GroundKind;
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
	Add(15.5, 4, -36.0, -10.0, TEXT("mg"), TEXT("")); // a machine gun's burst, tracer
	Add(16.0, 5, 24.0, 18.0, TEXT(""), TEXT("")); // a wreck burning
	Add(18.0, 4, -36.0, -10.0, TEXT("mg"), TEXT(""));
	Add(19.0, 6, 0.0, 60.0, TEXT("heavy"), TEXT("")); // a heavy flak burst hanging 60 m up over the far row
	// (the tank gun fires after the machine gun's two bursts, so those are seen clean from every side: at 15.0 s its
	// smoke lay over the gun's riflemen (build 1746). A third burst half a second after it shows the men in its dust)
	Add(20.0, 3, -40.0, -6.0, TEXT("tank_gun"), TEXT("")); // a tank gun firing toward the row
	Add(20.5, 4, -36.0, -10.0, TEXT("mg"), TEXT(""));
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
			Muzzle(Gun, (Target - Gun).GetSafeNormal(), Shot.Size, TestGround);
			break;
		}
		case 4:
		{
			const FVector Gun = Where + FVector(0.0, 0.0, 0.6 * UnitsPerMetre);
			const FVector Target = TestCentre + FVector(20.0, 20.0, 0.5) * UnitsPerMetre;
			Shots(13, Gun, Target, Shot.Size, 25, 15.0f, TestGround); // (gun 13's belt starts on a tracer, so the first frames show one)
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
