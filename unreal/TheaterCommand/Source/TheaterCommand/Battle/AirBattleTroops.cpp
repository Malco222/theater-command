// A raid on troops in 3D (Brian, 24 November 2026: "i want every air battle playable" and "if a bomber group is bombing
// troops instead of a factory or town and they get intercepted than i want that playable"). Where the sky has marks (the
// every-kind contract's place.marks: an army's column, laid by the rules, kept by the flight core as AirShow's Marks()),
// the column stands on the land under the planes: its tanks, lorries, carriers and guns as its army's models, its blocks
// of men as a few of its riflemen, each faced along its road. Every hit the rules count shows on the piece it struck, as
// the core lands the drawn bomb or the strafing pass's rounds on it (MarkHit): a vehicle set burning or wrecked goes up
// and burns, a damaged one smokes, a wreck wears its burnt shape, a block of men thins and scatters. The column's light
// flak fires back (MarkGun) through the effects kit, as every other gun. Drawn only: nothing here decides anything.
//
// Behind the Battle rules card's "Fly raids on troops yourself" (FLandLive's rule "flyTroops", off until it passes): a
// campaign raid on troops is offered to him only with that row on (AAirBattle::CampaignLiveOn(Kind)). A raid with no
// marks (every raid there is today) never reaches any of this: each hook returns at once.

#include "Battle/AirBattle.h"

#include "Battle/AirBattleState.h"
#include "Battle/AirMarks.h"
#include "Battle/AirShow.h"
#include "Battle/LandLive.h"
#include "Battle/ShipGuns.h"
#include "Campaign/CampaignMiniatures.h"
#include "Components/AudioComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "MeshDescription.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Sound/SoundBase.h"
#include "StaticMeshAttributes.h"
#include "TheaterCommand.h"

#include <cctype>
#include <string>

namespace
{
	FString TroopsWide(const std::string& Text)
	{
		return FString(UTF8_TO_TCHAR(Text.c_str()));
	}

	/** The column's nation as the effects kit colours its guns' tracers (as AirParts::KitNation in AirBattle.cpp: us, gb,
	 * de, it, jp, su; anything else the kit's own). */
	FString TroopsKitNation(const std::string& Nation)
	{
		std::string Up;
		for (const char Ch : Nation) Up += static_cast<char>(std::toupper(static_cast<unsigned char>(Ch)));
		if (Up == "USA" || Up == "US") return TEXT("us");
		if (Up == "UK" || Up == "GBR" || Up == "GB" || Up == "ENG") return TEXT("gb");
		if (Up == "GER" || Up == "DE") return TEXT("de");
		if (Up == "ITA" || Up == "IT") return TEXT("it");
		if (Up == "JAP" || Up == "JPN" || Up == "JP") return TEXT("jp");
		if (Up == "SOV" || Up == "USSR" || Up == "SU") return TEXT("su");
		return FString();
	}

	/** A gun firing in the effects kit, with its nation where the kit takes it (as AirParts::AirGun in AirBattle.cpp: an
	 * older kit draws its one colour). */
	template <typename TKit>
	void TroopsGun(TKit* Kit, double Now, int32 Gun, const FVector& Muzzle, const FVector& Aim, const FString& Kind, float Rate, float Mv, const FString& Nation)
	{
		if constexpr (requires(TKit* K) { K->AirGun(0.0, 0, FVector::ZeroVector, FVector::ZeroVector, FVector::ZeroVector, FString(), 0.0f, 0.0f, FString()); })
		{
			Kit->AirGun(Now, Gun, Muzzle, Aim, FVector::ZeroVector, Kind, Rate, Mv, Nation);
		}
		else
		{
			Kit->AirGun(Now, Gun, Muzzle, Aim, FVector::ZeroVector, Kind, Rate, Mv);
		}
	}

	/** An object of the mix by its dotted path (as AirParts::MixObject in AirBattle.cpp). */
	TSharedPtr<FJsonObject> TroopsMix(const TSharedPtr<FJsonObject>& Mix, const FString& Path)
	{
		TArray<FString> Steps;
		Path.ParseIntoArray(Steps, TEXT("."));
		TSharedPtr<FJsonObject> At = Mix;
		for (const FString& Step : Steps)
		{
			const TSharedPtr<FJsonObject>* Next = nullptr;
			if (!At.IsValid() || !At->TryGetObjectField(Step, Next))
			{
				return nullptr;
			}
			At = *Next;
		}
		return At;
	}

	/** A number from the mix, at a path of fields ("air.sameTakeGap"), or Default (as AirParts::MixNumber in
	 * AirBattle.cpp, file-local there). */
	double TroopsMixNumber(const TSharedPtr<FJsonObject>& Mix, const FString& Path, double Default)
	{
		TArray<FString> Steps;
		Path.ParseIntoArray(Steps, TEXT("."));
		TSharedPtr<FJsonObject> At = Mix;
		for (int32 K = 0; At.IsValid() && K < Steps.Num(); ++K)
		{
			if (K == Steps.Num() - 1)
			{
				double Value = Default;
				return At->TryGetNumberField(Steps[K], Value) ? Value : Default;
			}
			const TSharedPtr<FJsonObject>* Next = nullptr;
			At = At->TryGetObjectField(Steps[K], Next) ? *Next : nullptr;
		}
		return Default;
	}

	/** A shape built at its true size: its points, facings and colours (sRGB bytes, as FMiniatureMesh makes them). */
	struct FTroopShape
	{
		TArray<FVector> Points;
		TArray<int32> Triangles;
		TArray<FVector> Normals;
		TArray<FColor> Colors;
	};

	/** A model made into a shape standing on its own origin: its middle over it, its lowest point on it (so a piece set
	 * down on the land stands on it, whatever the model's own origin). */
	bool TroopShapeOf(const FMiniatureMesh& Made, FTroopShape& Out)
	{
		if (Made.Vertices.Num() == 0 || Made.Triangles.Num() < 3 || Made.Colors.Num() != Made.Vertices.Num())
		{
			return false;
		}
		const FBox Box(Made.Vertices);
		const FVector Foot(Box.GetCenter().X, Box.GetCenter().Y, Box.Min.Z);
		Out.Points.Reset(Made.Vertices.Num());
		for (const FVector& Point : Made.Vertices)
		{
			Out.Points.Add(Point - Foot);
		}
		Out.Triangles = Made.Triangles;
		Out.Normals = Made.Normals;
		Out.Colors = Made.Colors;
		return true;
	}

	/** A wreck's shape: the same, its paint burnt to soot and rust (the art director's to judge). */
	FTroopShape TroopBurnt(const FTroopShape& Whole)
	{
		FTroopShape Burnt = Whole;
		const FLinearColor Soot(0.05f, 0.045f, 0.04f, 1.0f), Rust(0.22f, 0.11f, 0.06f, 1.0f);
		for (int32 I = 0; I < Burnt.Colors.Num(); ++I)
		{
			const FLinearColor Was = FLinearColor::FromSRGBColor(Burnt.Colors[I]);
			const FLinearColor Into = (I % 7 == 0) ? Rust : Soot;
			Burnt.Colors[I] = FMath::Lerp(Was, Into, 0.85f).ToFColor(true);
		}
		return Burnt;
	}

	/**
	 * A shape as a mesh the engine keeps, all on Ink, with no collision; null for a shape with no triangles. The same
	 * build as the aircraft's copies' (AirParts::StillOf in AirBattle.cpp, file-local there), copied here.
	 */
	UStaticMesh* TroopStillOf(UObject* Owner, const FTroopShape& Shape, UMaterialInterface* Ink)
	{
		const int32 Count = Shape.Points.Num();
		if (!Owner || !Ink || Count == 0 || Shape.Triangles.Num() < 3 || Shape.Colors.Num() != Count)
		{
			return nullptr;
		}
		FMeshDescription D;
		FStaticMeshAttributes Attributes(D);
		Attributes.Register();
		auto Positions = Attributes.GetVertexPositions();
		auto Normals = Attributes.GetVertexInstanceNormals();
		auto Tangents = Attributes.GetVertexInstanceTangents();
		auto Signs = Attributes.GetVertexInstanceBinormalSigns();
		auto Colours = Attributes.GetVertexInstanceColors();
		auto Uvs = Attributes.GetVertexInstanceUVs();
		Uvs.SetNumChannels(1);
		const FPolygonGroupID Group = D.CreatePolygonGroup();
		Attributes.GetPolygonGroupMaterialSlotNames()[Group] = TEXT("Ink");
		D.ReserveNewVertices(Count);
		D.ReserveNewVertexInstances(Count);
		D.ReserveNewTriangles(Shape.Triangles.Num() / 3);
		TArray<FVertexInstanceID> Corners;
		Corners.SetNum(Count);
		for (int32 I = 0; I < Count; ++I)
		{
			const FVertexID Vertex = D.CreateVertex();
			Positions[Vertex] = FVector3f(Shape.Points[I]);
			const FVertexInstanceID Corner = D.CreateVertexInstance(Vertex);
			const FVector3f Normal = Shape.Normals.IsValidIndex(I) ? FVector3f(Shape.Normals[I]).GetSafeNormal(1e-6f, FVector3f::UpVector) : FVector3f::UpVector;
			Normals[Corner] = Normal;
			// (no normal map: any tangent across the facing will do)
			const FVector3f Across = FMath::Abs(Normal.Z) < 0.9f ? FVector3f::UpVector : FVector3f::ForwardVector;
			Tangents[Corner] = FVector3f::CrossProduct(Across, Normal).GetSafeNormal(1e-6f, FVector3f::ForwardVector);
			Signs[Corner] = 1.0f;
			Colours[Corner] = FVector4f(FLinearColor(Shape.Colors[I]));
			Uvs.Set(Corner, 0, FVector2f::ZeroVector);
			Corners[I] = Corner;
		}
		int32 Laid = 0;
		for (int32 T = 0; T + 2 < Shape.Triangles.Num(); T += 3)
		{
			const int32 A = Shape.Triangles[T], B = Shape.Triangles[T + 1], C = Shape.Triangles[T + 2];
			if (!Corners.IsValidIndex(A) || !Corners.IsValidIndex(B) || !Corners.IsValidIndex(C) || A == B || B == C || A == C)
			{
				continue;
			}
			const FVertexInstanceID Three[3] = { Corners[A], Corners[B], Corners[C] };
			D.CreateTriangle(Group, TArrayView<const FVertexInstanceID>(Three, 3));
			++Laid;
		}
		if (Laid == 0)
		{
			return nullptr;
		}
		UStaticMesh* Made = NewObject<UStaticMesh>(Owner, MakeUniqueObjectName(Owner, UStaticMesh::StaticClass(), FName(TEXT("AirTroopStill"))));
		Made->GetStaticMaterials().Add(FStaticMaterial(Ink, TEXT("Ink")));
		TArray<const FMeshDescription*> Levels;
		Levels.Add(&D);
		UStaticMesh::FBuildMeshDescriptionsParams Params;
		Params.bMarkPackageDirty = false;
		Params.bBuildSimpleCollision = false;
		Params.bCommitMeshDescription = false;
		Params.bFastBuild = true;
		Made->BuildFromMeshDescriptions(Levels, Params);
		return Made;
	}

	/** Whether the game has a model of that name (FMiniatureMesh::Vehicle: "Matilda", "Truck:UK", "25pdr"). */
	bool TroopModelThere(const FString& Name)
	{
		static TMap<FString, bool> Known;
		if (const bool* Have = Known.Find(Name))
		{
			return *Have;
		}
		FMiniatureMesh Probe;
		const bool bThere = !Name.IsEmpty() && Probe.Vehicle(Name, FVector::ZeroVector, 1.0, 0.0, false) && Probe.Vertices.Num() > 0;
		Known.Add(Name, bThere);
		return bThere;
	}

	/**
	 * The model a piece stands as: the rules' own name for it where the game has that model (in its nation's paint
	 * where it has one), else its army's by what it is: a tank its line tank (FMiniatureMesh::MainTankOf, by the
	 * campaign's year), a half-track its carrier, a lorry, a car or a horse-drawn wagon its lorry, a towed gun its field
	 * gun. A light flak gun ("aa") stands as its army's field gun until a flak gun is modelled (flagged for the art
	 * director). Empty for what is drawn as nothing yet (the fieldworks of the contract's step 2).
	 */
	FString TroopModelOf(const AirMarks::Piece& P, const FString& Nation)
	{
		const FString Asked = TroopsWide(P.Model);
		auto Painted = [&Nation](const FString& Name)
		{
			if (Name.IsEmpty())
			{
				return FString();
			}
			if (!Name.Contains(TEXT(":")) && !Nation.IsEmpty() && TroopModelThere(Name + TEXT(":") + Nation))
			{
				return Name + TEXT(":") + Nation;
			}
			return TroopModelThere(Name) ? Name : FString();
		};
		FString Found = Painted(Asked);
		if (!Found.IsEmpty())
		{
			return Found;
		}
		FString ByKind;
		if (P.What == "tank")
		{
			ByKind = FMiniatureMesh::MainTankOf(Nation);
		}
		else if (P.What == "halftrack")
		{
			ByKind = FMiniatureMesh::MainCarrierOf(Nation);
		}
		else if (P.What == "lorry" || P.What == "car" || P.What == "wagon")
		{
			ByKind = TEXT("Truck:") + Nation;
		}
		else if (P.What == "gun" || P.What == "aa")
		{
			ByKind = FMiniatureMesh::MainGunOf(Nation);
		}
		return Painted(ByKind);
	}

	/** How wide a piece's fire burns, metres, by what it is. */
	float TroopFireAcross(const std::string& What)
	{
		if (What == "tank") return 5.0f;
		if (What == "lorry" || What == "halftrack") return 4.0f;
		if (What == "car" || What == "wagon") return 3.0f;
		return 2.5f;
	}

	/** A whole number from a piece's id and a figure's place in its block: where a scattered man lies and how he is
	 * turned (no draw of the battle's own: every frame the same). */
	uint32 TroopsHash(uint32 A, uint32 B)
	{
		uint32 H = A * 2654435761u ^ (B * 0x9E3779B9u + 0x7F4A7C15u);
		H ^= H >> 16;
		H *= 0x85EBCA6Bu;
		H ^= H >> 13;
		return H;
	}
}

bool AAirBattle::CampaignLiveOn(const FString& Kind)
{
	if (Kind == TEXT("bomb") || Kind == TEXT("superiority"))
	{
		return CampaignLiveOn();
	}
	if (Kind == TEXT("ground"))
	{
		return FLandLive::RuleNow(TEXT("flyTroops"));
	}
	return false;
}

void AAirBattle::OpenTroops()
{
	TroopLooks.Reset();
	if (!State.IsValid() || !State->Show.Marks().Has() || !PlainInk)
	{
		return;
	}
	TRACE_CPUPROFILER_EVENT_SCOPE(AirBattle_OpenTroops);
	const double Began = FPlatformTime::Seconds();
	const AirMarks::Column& Column = State->Show.Marks();
	const double U = UnitsPerMetre;
	TroopsNation = TroopsKitNation(Column.Nat);
	int32 Vehicles = 0, Blocks = 0, Unmade = 0;
	TroopLooks.Reserve(static_cast<int32>(Column.Pieces.size()));
	for (const AirMarks::Piece& P : Column.Pieces)
	{
		FTroopLook& Look = TroopLooks.AddDefaulted_GetRef();
		const FString Nation = TroopsWide(P.Nat.empty() ? Column.Nat : P.Nat);
		if (P.What == "men")
		{
			// A block of men: its nation's rifleman, one mesh for every block of the nation.
			Look.bMen = true;
			Look.Key = TEXT("troops|man|") + Nation;
			TObjectPtr<UStaticMesh>* Have = TroopStills.Find(Look.Key);
			if (!Have)
			{
				FMiniatureMesh Made;
				Made.Soldier(Nation, FVector::ZeroVector, U, 0.0, false);
				FTroopShape Shape;
				Have = &TroopStills.Add(Look.Key, TroopShapeOf(Made, Shape) ? TroopStillOf(this, Shape, PlainInk) : nullptr);
			}
			Look.Mesh = Have->Get();
			Blocks += Look.Mesh.IsValid() ? 1 : 0;
			Unmade += Look.Mesh.IsValid() ? 0 : 1;
			continue;
		}
		const FString Model = TroopModelOf(P, Nation);
		if (Model.IsEmpty())
		{
			++Unmade; // (the fieldworks' words, kept and drawn as nothing yet; or a model the game has not)
			continue;
		}
		Look.Key = TEXT("troops|") + Model + TEXT("|ok");
		Look.BurntKey = TEXT("troops|") + Model + TEXT("|burnt");
		if (!TroopStills.Contains(Look.Key))
		{
			// (each model's meshes made once, as it stands and burnt, at its true size, its nation's paint)
			FMiniatureMesh Made;
			Made.Vehicle(Model, FVector::ZeroVector, U, 0.0, true);
			FTroopShape Shape;
			const bool bShape = TroopShapeOf(Made, Shape);
			TroopStills.Add(Look.Key, bShape ? TroopStillOf(this, Shape, PlainInk) : nullptr);
			TroopStills.Add(Look.BurntKey, bShape ? TroopStillOf(this, TroopBurnt(Shape), PlainInk) : nullptr);
		}
		const TObjectPtr<UStaticMesh>* Whole = TroopStills.Find(Look.Key);
		const TObjectPtr<UStaticMesh>* Burnt = TroopStills.Find(Look.BurntKey);
		Look.Mesh = Whole ? Whole->Get() : nullptr;
		Look.Burnt = Burnt ? Burnt->Get() : nullptr;
		Vehicles += Look.Mesh.IsValid() ? 1 : 0;
		Unmade += Look.Mesh.IsValid() ? 0 : 1;
	}
	UE_LOG(LogTheater, Display, TEXT("Air battle: a raid on troops: the column under it, %d pieces (%d vehicles and guns, %d blocks of men, %d drawn as nothing), %d meshes, %s %s, made in %.0f ms."),
		TroopLooks.Num(), Vehicles, Blocks, Unmade, TroopStills.Num(), *TroopsWide(Column.Nat), *TroopsWide(Column.Posture), (FPlatformTime::Seconds() - Began) * 1000.0);
}

void AAirBattle::DrawTroops(const FVector& EyeAt)
{
	if (TroopLooks.Num() == 0 || !State.IsValid())
	{
		return;
	}
	TRACE_CPUPROFILER_EVENT_SCOPE(AirBattle_DrawTroops);
	FAirBattleState& S = *State;
	const AirMarks::Column& Column = S.Show.Marks();
	if (static_cast<int32>(Column.Pieces.size()) != TroopLooks.Num())
	{
		return;
	}
	const double U = UnitsPerMetre;
	// (the models are built nose toward -X with the right hand at -Y, as the aircraft: turned half round before their face)
	const FQuat HalfRound = FRotator(0.0, 180.0, 0.0).Quaternion();
	const double Far = TroopsFarM * U;
	const int32 Per = FMath::Max(1, Column.Per);
	TroopsDrawn = 0;
	int32 Figures = 0;
	for (int32 I = 0; I < TroopLooks.Num(); ++I)
	{
		const AirMarks::Piece& P = Column.Pieces[static_cast<size_t>(I)];
		FTroopLook& Look = TroopLooks[I];
		const double Floor = S.Show.Ground ? S.Show.Ground(P.X, P.Y) : 0.0;
		const FVector Base = World(P.X, P.Y, Floor);
		// What the piece's state calls for in the kit, laid once (wherever the eye is: the kit draws to its own reach): a
		// vehicle or gun burning or wrecked, a fire that burns on through the raid; damaged, a small flame gone at once
		// and its thin smoke after it. A block of men has none.
		const int32 Wants = Look.bMen ? 0 : (P.Now == AirMarks::State::Burn || P.Now == AirMarks::State::Out) ? 2 : P.Now == AirMarks::State::Hit ? 1 : 0;
		if (Wants > Look.Laid)
		{
			if (FBattleEffects* Kit = GroundKit())
			{
				const FVector Deck = Base + FVector(0.0, 0.0, 1.2 * U);
				if (Wants == 2)
				{
					Kit->Burn(Deck, TroopFireAcross(P.What), 600.0f);
				}
				else
				{
					Kit->Burn(Deck, 1.0f, 4.0f);
				}
				Look.Laid = Wants; // (laid once the kit is there: before it, tried again next frame)
			}
		}
		if (FVector::DistSquared(Base, EyeAt) > Far * Far || !Look.Mesh.IsValid())
		{
			continue;
		}
		const FQuat Facing = FRotator(0.0, FMath::RadiansToDegrees(P.Face), 0.0).Quaternion() * HalfRound;
		if (Look.bMen)
		{
			// A block of men: one figure for each Per men left, in two files along the road; scattered off it once a hit
			// has scattered them or cost them men (each where its own hash lays it, lying every way).
			const int32 Count = FMath::Clamp((FMath::Max(0, P.Men) + Per - 1) / Per, 0, TroopsMostFigures);
			if (Count == 0)
			{
				continue;
			}
			FPlaneCopies* Group = CopiesFor(Look.Key, false, Look.Mesh.Get(), PlainInk);
			if (!Group)
			{
				continue;
			}
			const bool bScattered = Look.bScattered || P.Men < P.N;
			const double AheadX = FMath::Sin(P.Face), AheadY = FMath::Cos(P.Face);   // along the road (east, north)
			const double RightX = AheadY, RightY = -AheadX;                         // its right
			const int32 Ranks = (Count + 1) / 2;
			for (int32 K = 0; K < Count; ++K)
			{
				double Along = ((K / 2 + 0.5) / Ranks - 0.5) * P.Long * 0.9;
				double Across = (K % 2 == 0 ? -0.5 : 0.5) * FMath::Min(P.Wide, 3.0);
				double Turn = 0.0;
				if (bScattered)
				{
					const uint32 H = TroopsHash(static_cast<uint32>(P.Id), static_cast<uint32>(K));
					Along += ((H & 0xFF) / 255.0 - 0.5) * P.Long * 0.6;
					Across += (((H >> 8) & 0xFF) / 255.0 - 0.5) * P.Wide * 5.0;
					Turn = ((H >> 16) & 0xFF) / 255.0 * 360.0;
				}
				const FVector At = World(P.X + AheadX * Along + RightX * Across, P.Y + AheadY * Along + RightY * Across, Floor);
				const FTransform Here(Facing * FRotator(0.0, Turn, 0.0).Quaternion(), At);
				Group->Keys.Add(P.Id);
				Group->At.Add(Here);
				Group->Before.Add(Here);
				++Figures;
			}
			++TroopsDrawn;
			continue;
		}
		// A vehicle or a gun: as it stands, or burnt where it is wrecked.
		const bool bBurnt = P.Now == AirMarks::State::Out && Look.Burnt.IsValid();
		FPlaneCopies* Group = bBurnt ? CopiesFor(Look.BurntKey, true, Look.Burnt.Get(), PlainInk) : CopiesFor(Look.Key, true, Look.Mesh.Get(), PlainInk);
		if (!Group)
		{
			continue;
		}
		const FTransform Here(Facing, Base);
		Group->Keys.Add(P.Id);
		Group->At.Add(Here);
		Group->Before.Add(Here);
		++TroopsDrawn;
	}
	if (RealT >= TroopsLogAt)
	{
		TroopsLogAt = RealT + 60.0;
		UE_LOG(LogTheater, Display, TEXT("Air battle: the column: %d of its %d pieces drawn within %.0f km of the eye (%d men figures)."),
			TroopsDrawn, TroopLooks.Num(), TroopsFarM / 1000.0, Figures);
	}
}

void AAirBattle::PlayTroops(const AirShow::Fx& E)
{
	if (TroopLooks.Num() == 0 || !State.IsValid())
	{
		return;
	}
	FAirBattleState& S = *State;
	const double U = UnitsPerMetre;
	const FVector At = World(E.At.X, E.At.Y, E.At.Z);
	if (E.Kind == AirShow::FxKind::MarkHit)
	{
		// A hit the rules counted, shown as its bomb or its rounds arrive (the core's MarkHit, at the piece on the ground;
		// its state is the core's already, and DrawTroops lays its fire). Here only what happens at the moment: a vehicle
		// set burning or wrecked has its fuel go up; a strafing pass's burn or wreck leaves its scorch (a bomb's crater
		// is laid where it fell, with its BombHit); a block of men scattered. A hit the rules call big (extra.big, the
		// core's Part "big") goes up half as wide again and scorches twice the ground (the art director's to judge).
		const int32 Index = S.Show.Marks().IndexOf(E.Key);
		if (!TroopLooks.IsValidIndex(Index))
		{
			return;
		}
		FTroopLook& Look = TroopLooks[Index];
		const AirMarks::Piece& P = S.Show.Marks().Pieces[static_cast<size_t>(Index)];
		if (E.Text == "scatter")
		{
			Look.bScattered = true;
		}
		if (!Look.bMen && (E.Text == "burn" || E.Text == "wreck") && P.What != "gun" && P.What != "aa")
		{
			if (FBattleEffects* Kit = GroundKit())
			{
				const double Across = (P.What == "lorry" ? 8.0 : 6.0) * (E.Part == "big" ? 1.5 : 1.0);
				const FVector Fuel = At + FVector(0.0, 0.0, 1.5 * U);
				Kit->FuelBall(Fuel, static_cast<float>(Across));
				NoteFuelBall(Fuel, Across);
			}
		}
		if (E.What != "bomb" && (E.Text == "burn" || E.Text == "wreck" || E.Text == "blast"))
		{
			ScarLand(E.At.X, E.At.Y, E.Part == "big" ? 120.0 : 60.0);
		}
		UE_LOG(LogTheater, Display, TEXT("Air battle: a hit on the column: %s %d, %s (%s), %d men; now %s."), *TroopsWide(P.What), P.Id, *TroopsWide(E.Text),
			*TroopsWide(E.What), static_cast<int32>(E.Size), *TroopsWide(AirMarks::WordOf(P.Now)));
		return;
	}
	if (E.Kind != AirShow::FxKind::MarkGun)
	{
		return;
	}
	// The column's light flak firing back (drawn only): its stream in the kit for the muzzle's height, as an aircraft's
	// gun's (the Gun case in AirBattle.cpp); GunStop there takes it out of the kit and cuts its take.
	FBattleEffects* const* Was = GunKits.Find(E.Key);
	FBattleEffects* const Before = Was ? *Was : nullptr;
	FBattleEffects* const Kit = KitAt(At, Before);
	if (Before && Before != Kit)
	{
		Before->AirGunStop(E.Key);
	}
	if (Kit)
	{
		GunKits.Add(E.Key, Kit);
		TroopsGun(Kit, S.Show.AirNow(), E.Key, At, World(E.Aim.X, E.Aim.Y, E.Aim.Z), TroopsWide(E.What), static_cast<float>(E.Rate), static_cast<float>(E.Mv * U), TroopsNation);
	}
	// Heard at the gun (Brian, 21 November 16:21Z: "when ever there is gun fire make sure the sound of the guns happens"):
	// the mix's cannon take laid end to end while it fires, the next due as its part ends.
	const double* Due = GunHeardAt.Find(E.Key);
	if (Due && RealT < *Due)
	{
		return;
	}
	const TSharedPtr<FJsonObject> Cannon = TroopsMix(Mix, TEXT("air.gunsByCalibre.cannon"));
	FString Take;
	float Cut = 0.0f;
	if (Cannon.IsValid())
	{
		Cannon->TryGetStringField(TEXT("take"), Take);
		const TArray<TSharedPtr<FJsonValue>>* Part = nullptr;
		if (Cannon->TryGetArrayField(TEXT("part"), Part) && Part->Num() == 2)
		{
			Cut = static_cast<float>((*Part)[1]->AsNumber());
		}
	}
	USoundBase* const Recording = Take.IsEmpty() ? nullptr : SoundOf(*Take);
	double Again = 0.9;
	if (Recording)
	{
		const double Whole = Recording->GetDuration();
		Again = Cut > 0.0f ? Cut : (Whole > 0.0 && Whole < 10.0 ? Whole : 0.9);
	}
	// (never sooner than the mix's own gap after itself, as every other gun's: air.sameTakeGap)
	Again = FMath::Clamp(Again - 0.02, FMath::Max(0.3, TroopsMixNumber(Mix, TEXT("air.sameTakeGap"), 0.15)), 2.0);
	GunHeardAt.Add(E.Key, RealT + Again);
	if (!Recording)
	{
		return;
	}
	if (UAudioComponent* Played = Sound(*Take, At, TEXT("air_guns"), 1.0f, 0.0f, Cut, 0.0f, 1.0f))
	{
		// (its takes still playing kept, the finished ones let go, as the Gun case keeps an aircraft's)
		TArray<TWeakObjectPtr<UAudioComponent>> Had;
		GunTakes.MultiFind(E.Key, Had);
		GunTakes.Remove(E.Key);
		for (const TWeakObjectPtr<UAudioComponent>& Still : Had)
		{
			if (Still.IsValid() && Still->IsPlaying())
			{
				GunTakes.Add(E.Key, Still);
			}
		}
		GunTakes.Add(E.Key, Played);
	}
}

void AAirBattle::EndTroops()
{
	if (TroopLooks.Num() == 0)
	{
		return;
	}
	// The column's guns stopped in their kits before the kits end, and their takes cut.
	TArray<int32> Guns;
	for (const TPair<int32, FBattleEffects*>& Pair : GunKits)
	{
		if (Pair.Key >= AirShow::MarkGunBase && !ShipGuns::IsStream(Pair.Key))
		{
			Guns.Add(Pair.Key);
		}
	}
	for (const int32 Key : Guns)
	{
		FBattleEffects* Kit = nullptr;
		if (GunKits.RemoveAndCopyValue(Key, Kit) && Kit)
		{
			Kit->AirGunStop(Key);
		}
		TArray<TWeakObjectPtr<UAudioComponent>> Playing;
		GunTakes.MultiFind(Key, Playing);
		for (const TWeakObjectPtr<UAudioComponent>& Take : Playing)
		{
			if (Take.IsValid())
			{
				Take->Stop();
			}
		}
		GunTakes.Remove(Key);
		GunHeardAt.Remove(Key);
	}
	TroopLooks.Reset();
}
