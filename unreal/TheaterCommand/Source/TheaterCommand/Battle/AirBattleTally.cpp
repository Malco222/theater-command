// Named aces (Brian's, release 2; the air thread's extra.ace, 22 November): an ace's victories painted on his group's
// lead aircraft, one mark a kill in the victims' symbols, as the modeller's rules lay a bomber's record on her side
// (battle-models/air/tally/README.md; Battle/AirTally). Each mark a small square of the tally sheet (Content/Data/Tally,
// from the modeller's, passed by the art director on 20 November) on one of the model's kill slots, upright on its skin
// with its right toward the tail, drawn with M_TallyMark over the paint under it. Shown, never counted.

#include "Battle/AirBattle.h"

#include "Battle/AirBattleState.h"
#include "Battle/AirShow.h"
#include "Battle/AirTally.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Campaign/CampaignTextures.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "ProceduralMeshComponent.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "TheaterCommand.h"

#include <map>
#include <string>
#include <vector>

namespace
{
	/** The test ace ("-TheaterAceTest=att:6:GER=6", or def, its victims' nations after the kills, and a title after
	 * those if wanted, as the rules give one: "Flight Lieutenant Tom Harland"; else his nation's sample): Side -1 when
	 * none is asked. */
	struct FTestAce
	{
		int32 Side = -1;
		int32 Kills = 0;
		std::map<std::string, int> By;
		std::string Name;
	};

	const FTestAce& TestAce()
	{
		static const FTestAce Asked = []
		{
			FTestAce Out;
			FString Text;
			if (!FParse::Value(FCommandLine::Get(), TEXT("TheaterAceTest="), Text, false))
			{
				return Out;
			}
			TArray<FString> Parts;
			Text.ParseIntoArray(Parts, TEXT(":"), false);
			if (Parts.Num() < 2)
			{
				return Out;
			}
			Out.Side = Parts[0].TrimStartAndEnd().Equals(TEXT("def"), ESearchCase::IgnoreCase) ? 1 : 0;
			Out.Kills = FMath::Clamp(FCString::Atoi(*Parts[1]), 0, 99);
			if (Parts.Num() > 2)
			{
				TArray<FString> Each;
				Parts[2].ParseIntoArray(Each, TEXT(","), true);
				for (const FString& One : Each)
				{
					FString Nation, Count;
					if (One.Split(TEXT("="), &Nation, &Count))
					{
						Out.By[TCHAR_TO_UTF8(*Nation.TrimStartAndEnd())] = FCString::Atoi(*Count);
					}
				}
			}
			if (Parts.Num() > 3 && !Parts[3].IsEmpty())
			{
				Out.Name = TCHAR_TO_UTF8(*Parts[3]);
			}
			UE_LOG(LogTheater, Display, TEXT("Air battle: aces: a test ace with %d victories in the first fighter group of the %s."),
				Out.Kills, Out.Side == 1 ? TEXT("defence") : TEXT("raid"));
			return Out;
		}();
		return Asked;
	}
}

bool AAirBattle::TallyReady()
{
	if (bTallyTried)
	{
		return TallyInk != nullptr;
	}
	bTallyTried = true;
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Campaign/Materials/M_TallyMark.M_TallyMark"));
	if (!Base)
	{
		UE_LOG(LogTheater, Warning, TEXT("Air battle: aces: no M_TallyMark (Tools/build_content.sh makes it), so no ace's victories are painted."));
		return false;
	}
	// The sheet: plain RGBA bytes (Tools/tally_sheet.py), and the modeller's index of its cells.
	const FString Folder = FPaths::ProjectContentDir() / TEXT("Data") / TEXT("Tally");
	FString Index;
	TSharedPtr<FJsonObject> Sheet;
	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToString(Index, *(Folder / TEXT("tally-sheet.json"))) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Index), Sheet)
		|| !Sheet.IsValid() || !FFileHelper::LoadFileToArray(Bytes, *(Folder / TEXT("tally-sheet.rgba"))))
	{
		UE_LOG(LogTheater, Warning, TEXT("Air battle: aces: the tally sheet is missing (Content/Data/Tally), so no ace's victories are painted."));
		return false;
	}
	const int32 Width = static_cast<int32>(Sheet->GetNumberField(TEXT("width")));
	const int32 Height = static_cast<int32>(Sheet->GetNumberField(TEXT("height")));
	if (Width <= 0 || Height <= 0 || Bytes.Num() != Width * Height * 4)
	{
		UE_LOG(LogTheater, Warning, TEXT("Air battle: aces: the tally sheet is %d bytes, not %d by %d, so no ace's victories are painted."), Bytes.Num(), Width, Height);
		return false;
	}
	TallySpan = Sheet->HasField(TEXT("span")) ? Sheet->GetNumberField(TEXT("span")) : 1.25;
	const TSharedPtr<FJsonObject>* Cells = nullptr;
	if (Sheet->TryGetObjectField(TEXT("uv"), Cells))
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Tag : (*Cells)->Values)
		{
			const TSharedPtr<FJsonObject>* Sets = nullptr;
			if (!Tag.Value.IsValid() || !Tag.Value->TryGetObject(Sets))
			{
				continue;
			}
			for (const TCHAR* Set : { TEXT("dark"), TEXT("light") })
			{
				const TArray<TSharedPtr<FJsonValue>>* Box = nullptr;
				if ((*Sets)->TryGetArrayField(Set, Box) && Box->Num() >= 4)
				{
					TallyCells.Add(Tag.Key + TEXT("|") + Set, FVector4((*Box)[0]->AsNumber(), (*Box)[1]->AsNumber(), (*Box)[2]->AsNumber(), (*Box)[3]->AsNumber()));
				}
			}
		}
	}
	TArray<FColor> Pixels;
	Pixels.SetNumUninitialized(Width * Height);
	for (int32 K = 0; K < Pixels.Num(); ++K)
	{
		Pixels[K] = FColor(Bytes[K * 4], Bytes[K * 4 + 1], Bytes[K * 4 + 2], Bytes[K * 4 + 3]);
	}
	// (colours as the eye sees them, its edge held, with its smaller copies: a mark seen from farther off is a soft
	// small mark, not a shimmer)
	TallySheet = CampaignTextures::Create(Width, Height, Pixels, true, false, TEXT("AirTallySheet"));
	if (!TallySheet || TallyCells.Num() == 0)
	{
		UE_LOG(LogTheater, Warning, TEXT("Air battle: aces: the tally sheet could not be made, so no ace's victories are painted."));
		return false;
	}
	TallyInk = UMaterialInstanceDynamic::Create(Base, this);
	TallyInk->SetTextureParameterValue(TEXT("Sheet"), TallySheet);
	TallyInk->SetVectorParameterValue(TEXT("Tin"), FLinearColor::White);
	TallyInk->SetScalarParameterValue(TEXT("Roughness"), PaintRoughness);
	TallyInk->SetScalarParameterValue(TEXT("Specular"), PaintSpecular);
	return true;
}

namespace
{
	/** The letters' index (Content/Data/Tally/name-letters.json), read once. */
	AirTally::Letters& NameHand()
	{
		static AirTally::Letters Hand;
		return Hand;
	}

	/** One part of an ace's record, one section of his tally's mesh: its squares' corners, their places on its sheet,
	 * the paint under each (its alpha the mark's strength), and its triangles. */
	struct FTallyPart
	{
		TArray<FVector> Corners3;
		TArray<FVector> Normals;
		TArray<FVector2D> Places;
		TArray<FColor> Colours;
		TArray<int32> Triangles;

		/** A square on her skin facing Out: its middle, its right R and up V, its half width and height, the sheet's
		 * box (u0, v0, u1, v1: its top left at the corner nearest her nose and the sky). */
		void Add(const FVector& Middle, const FVector& R, const FVector& V, double HalfW, double HalfH, const FVector4& Box, const FColor& Under, const FVector& Out)
		{
			const int32 First = Corners3.Num();
			Corners3.Append({ Middle - R * HalfW + V * HalfH, Middle + R * HalfW + V * HalfH, Middle + R * HalfW - V * HalfH, Middle - R * HalfW - V * HalfH });
			Places.Append({ FVector2D(Box.X, Box.Y), FVector2D(Box.Z, Box.Y), FVector2D(Box.Z, Box.W), FVector2D(Box.X, Box.W) });
			for (int32 K = 0; K < 4; ++K)
			{
				Normals.Add(Out);
				Colours.Add(Under);
			}
			// (each triangle's corners the way round the engine draws as facing out, as FMiniatureMesh::Face lays them)
			for (const FIntVector& Tri : { FIntVector(0, 1, 2), FIntVector(0, 2, 3) })
			{
				const FVector Facing = FVector::CrossProduct(Corners3[First + Tri.Y] - Corners3[First + Tri.Z], Corners3[First + Tri.X] - Corners3[First + Tri.Z]);
				if (FVector::DotProduct(Facing, Out) >= 0.0)
				{
					Triangles.Append({ First + Tri.X, First + Tri.Y, First + Tri.Z });
				}
				else
				{
					Triangles.Append({ First + Tri.X, First + Tri.Z, First + Tri.Y });
				}
			}
		}
	};

	/** Upright on her side: up along her skin toward the sky, right toward her tail (seen from outside her port side the
	 * tail is on the right, so a name reads from her nose aft). */
	void Upright(const FVector& Out, FVector& Right, FVector& Up)
	{
		Up = FVector::UpVector - Out * FVector::DotProduct(FVector::UpVector, Out);
		Up = Up.GetSafeNormal(UE_SMALL_NUMBER, FVector::BackwardVector);
		Right = FVector::CrossProduct(Out, Up).GetSafeNormal();
	}

	unsigned RgbOf(const FColor& C)
	{
		return (static_cast<unsigned>(C.R) << 16) | (static_cast<unsigned>(C.G) << 8) | static_cast<unsigned>(C.B);
	}

	FLinearColor TinOf(unsigned Rgb)
	{
		return FLinearColor(FColor(static_cast<uint8>((Rgb >> 16) & 0xFF), static_cast<uint8>((Rgb >> 8) & 0xFF), static_cast<uint8>(Rgb & 0xFF), 255));
	}
}

bool AAirBattle::LettersReady()
{
	if (bLettersTried)
	{
		return NameSheet != nullptr;
	}
	bLettersTried = true;
	if (!TallyReady())
	{
		return false;
	}
	// The sheet: one byte a pixel, how much of it is letter (Tools/name_letters.py), and its index; read as bytes, so
	// a name's letters keep their own spelling (UTF-8).
	const FString Folder = FPaths::ProjectContentDir() / TEXT("Data") / TEXT("Tally");
	TArray<uint8> Index, Bytes;
	AirTally::Letters& Hand = NameHand();
	if (!FFileHelper::LoadFileToArray(Index, *(Folder / TEXT("name-letters.json")))
		|| !AirTally::ReadLetters(std::string(reinterpret_cast<const char*>(Index.GetData()), static_cast<size_t>(Index.Num())), Hand)
		|| !FFileHelper::LoadFileToArray(Bytes, *(Folder / TEXT("name-letters.a8"))) || Bytes.Num() != Hand.Width * Hand.Height)
	{
		UE_LOG(LogTheater, Warning, TEXT("Air battle: aces: the letters' sheet is missing or not as its index says (Content/Data/Tally/name-letters), so no ace's name is painted."));
		return false;
	}
	// (white everywhere, so the smaller copies draw no dark rim; the ink is each name's own Tin)
	TArray<FColor> Pixels;
	Pixels.SetNumUninitialized(Bytes.Num());
	for (int32 K = 0; K < Bytes.Num(); ++K)
	{
		Pixels[K] = FColor(255, 255, 255, Bytes[K]);
	}
	NameSheet = CampaignTextures::Create(Hand.Width, Hand.Height, Pixels, true, false, TEXT("AirNameLetters"));
	if (!NameSheet)
	{
		UE_LOG(LogTheater, Warning, TEXT("Air battle: aces: the letters' sheet could not be made, so no ace's name is painted."));
	}
	return NameSheet != nullptr;
}

void AAirBattle::MakeTally(int32 Key, FPlaneMade& Made, UProceduralMeshComponent* Mesh, const AirShow::Group& Ace, const FString& Nation)
{
	Made.bTallyTried = true;
	const FAirTallySide* Side = TallyOfShape(Made.Shape);
	// (Germany's habit: her kills as white bars on her rudder, her name alone under the cockpit; the painter's nation
	// decides, as the modeller's marks.record does, so a Finnish Bf 109 wears her victims' marks on her side)
	const bool bBars = Side && Nation == TEXT("GER");
	const bool bKills = Side && !bBars && Side->At.Num() > 0 && Ace.AceKills > 0;
	const bool bRudder = Side && bBars && Side->Bars.Num() > 0 && Ace.AceKills > 0;
	const bool bName = Side && Side->Line.Num() >= 2 && !Ace.AceName.empty();
	if (!bKills && !bRudder && !bName)
	{
		UE_LOG(LogTheater, Display, TEXT("Air battle: aces: %s (%s) has no %s in its model, so %s flies unpainted."),
			*Made.Model, *Made.Shape, bBars ? TEXT("rudder bars or name line") : TEXT("kill slots or name line"), UTF8_TO_TCHAR(Ace.AceName.c_str()));
		return;
	}
	if (!TallyReady())
	{
		return;
	}
	const double U = UnitsPerMetre;
	const std::string Painter = TCHAR_TO_UTF8(*Nation);
	FTallyPart Parts[3];
	for (double& Size : Made.TallySize)
	{
		Size = 0.0;
	}
	// 0: his kills, in the victims' symbols, on her kill slots (the tally sheet).
	const TCHAR* Set = Side->bLight ? TEXT("light") : TEXT("dark");
	if (bKills)
	{
		const std::vector<AirTally::Mark> Marks = AirTally::Fill(Painter, 0, Ace.AceKills, Ace.AceBy, 0, Side->At.Num(), Key, Side->PerRow);
		for (const AirTally::Mark& Mark : Marks)
		{
			const FVector4* Cell = TallyCells.Find(FString(UTF8_TO_TCHAR(Mark.Tag.c_str())) + TEXT("|") + Set);
			if (!Mark.bKill || !Cell || !Side->At.IsValidIndex(Mark.Slot))
			{
				continue;
			}
			const FVector Out = Side->Out[Mark.Slot];
			// Upright on her side, then the painter's hand where the file gives it: turned by its lean (anticlockwise
			// seen from outside), sized, and moved along toward the tail and up her skin.
			FVector Right, Up;
			Upright(Out, Right, Up);
			FVector4 Hand(0.0, 1.0, 0.0, 0.0);
			if (Side->Hand.Num() > 0)
			{
				Hand = Side->Hand[(Mark.Slot + 37 * (Key & 0xFFFF)) % Side->Hand.Num()];
			}
			const double Lean = FMath::DegreesToRadians(Hand.X);
			const FVector R = Right * FMath::Cos(Lean) + Up * FMath::Sin(Lean);
			const FVector V = Up * FMath::Cos(Lean) - Right * FMath::Sin(Lean);
			const double Half = TallySpan * Side->Radius * Hand.Y;
			// (two millimetres off the skin, so it never sinks into a fuselage's curve)
			const FVector Middle = Side->At[Mark.Slot] + Out * (0.002 * U) + Right * Hand.Z + Up * Hand.W;
			FColor Under = Side->Skin.IsValidIndex(Mark.Slot) ? Side->Skin[Mark.Slot] : FColor(96, 96, 96);
			Under.A = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt32(Mark.Fade * 255.0), 0, 255));
			Parts[0].Add(Middle, R, V, Half, Half, *Cell, Under, Out);
		}
		Made.TallySize[0] = 2.0 * Side->Radius / U;
	}
	// 1: his name under the cockpit (the modeller's tally.name and acepanel's rules): each letter its own small square
	// at its place along the line, upright, facing her skin there; in one ink for the paint under the line.
	FString Painted;
	double PaintedCap = 0.0;
	unsigned Ink = 0;
	if (bName && LettersReady())
	{
		const AirTally::Letters& Hand = NameHand();
		const AirTally::NameLaid Laid = AirTally::LayName(AirTally::NameTries(Painter, Ace.AceName), Hand, Side->Long, Side->Cap, Side->CapMin);
		Ink = AirTally::NameInk(RgbOf(Side->NameSkin), Painter);
		const double CapU = Laid.Cap * Side->Scale;
		for (const AirTally::NameLetter& Letter : Laid.Letters)
		{
			const auto Found = Hand.Cells.find(Letter.Ch);
			if (Found == Hand.Cells.end())
			{
				continue;
			}
			// (where it falls on her line, between the two points about it)
			int32 K = 0;
			while (K + 2 < Side->LineAft.Num() && Side->LineAft[K + 1] < Letter.Middle)
			{
				++K;
			}
			const double Gap = Side->LineAft[K + 1] - Side->LineAft[K];
			const double T = Gap > 1e-9 ? FMath::Clamp((Letter.Middle - Side->LineAft[K]) / Gap, 0.0, 1.0) : 0.0;
			const FVector Out = FMath::Lerp(Side->LineOut[K], Side->LineOut[K + 1], T).GetSafeNormal(UE_SMALL_NUMBER, Side->LineOut[K]);
			const FVector On = FMath::Lerp(Side->Line[K], Side->Line[K + 1], T);
			FVector Right, Up;
			Upright(Out, Right, Up);
			// Its cell, trimmed across to its letter and a little room; the capitals' middle (the cell's row Mid) on the
			// line.
			const double CellX = Found->second.Col * Hand.CellW + 0.5 * Hand.CellW;
			const double HalfPx = FMath::Min(0.5 * Hand.CellW, (0.5 * Found->second.Wide + 0.2) * Hand.Cap);
			const FVector4 Box((CellX - HalfPx) / Hand.Width, static_cast<double>(Found->second.Row * Hand.CellH) / Hand.Height,
				(CellX + HalfPx) / Hand.Width, static_cast<double>((Found->second.Row + 1) * Hand.CellH) / Hand.Height);
			const double Above = static_cast<double>(Hand.Mid) / Hand.Cap * CapU;
			const double Below = static_cast<double>(Hand.CellH - Hand.Mid) / Hand.Cap * CapU;
			const FVector Middle = On + Out * (0.002 * U) + Up * (0.5 * (Above - Below));
			Parts[1].Add(Middle, Right, Up, HalfPx / Hand.Cap * CapU, 0.5 * (Above + Below), Box, Side->NameSkin, Out);
		}
		if (!Laid.Missing.empty())
		{
			UE_LOG(LogTheater, Display, TEXT("Air battle: aces: the letters' sheet has no %s, left out of %s's name."), UTF8_TO_TCHAR(Laid.Missing.c_str()), UTF8_TO_TCHAR(Ace.AceName.c_str()));
		}
		Painted = UTF8_TO_TCHAR(Laid.Text.c_str());
		PaintedCap = Laid.Cap;
		Made.TallySize[1] = CapU / U;
	}
	// 2: a German pilot's kills, a white bar each on her rudder, first from the front (the modeller's tally.rudder).
	int32 BarsPainted = 0;
	if (bRudder && LettersReady())
	{
		const AirTally::Letters& Hand = NameHand();
		const auto Found = Hand.Cells.find("BAR");
		if (Found != Hand.Cells.end())
		{
			// (the middle half of the solid cell, clear of its edge in the smaller copies)
			const double X0 = Found->second.Col * Hand.CellW, Y0 = Found->second.Row * Hand.CellH;
			const FVector4 Box((X0 + 0.25 * Hand.CellW) / Hand.Width, (Y0 + 0.25 * Hand.CellH) / Hand.Height,
				(X0 + 0.75 * Hand.CellW) / Hand.Width, (Y0 + 0.75 * Hand.CellH) / Hand.Height);
			BarsPainted = FMath::Min(Ace.AceKills, Side->Bars.Num());
			for (int32 K = 0; K < BarsPainted; ++K)
			{
				const FVector Out = Side->BarsOut[K];
				FVector Right, Up;
				Upright(Out, Right, Up);
				FColor Under = Side->BarsSkin.IsValidIndex(K) ? Side->BarsSkin[K] : FColor(96, 96, 96);
				Under.A = 255;
				// (the modeller's bar: 0.6 of its radius wide, its whole height twice the radius)
				Parts[2].Add(Side->Bars[K] + Out * (0.002 * U), Right, Up, 0.3 * Side->BarRadius, Side->BarRadius, Box, Under, Out);
			}
			Made.TallySize[2] = 2.0 * Side->BarRadius / U;
		}
	}
	if (Parts[0].Triangles.Num() + Parts[1].Triangles.Num() + Parts[2].Triangles.Num() == 0)
	{
		return;
	}
	UProceduralMeshComponent* Tally = NewObject<UProceduralMeshComponent>(this, MakeUniqueObjectName(this, UProceduralMeshComponent::StaticClass(), *FString::Printf(TEXT("AirTally_%d"), Key)));
	Tally->SetupAttachment(Mesh);
	Tally->SetMobility(EComponentMobility::Movable);
	Tally->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Tally->SetCastShadow(false);
	// (lit as her paint is, by the light thrown up from under the eye too)
	Tally->LightingChannels.bChannel1 = true;
	Tally->bUseAsyncCooking = false;
	Tally->RegisterComponent();
	for (int32 Section = 0; Section < 3; ++Section)
	{
		const FTallyPart& Part = Parts[Section];
		if (Part.Triangles.Num() == 0)
		{
			Made.TallySize[Section] = 0.0;
			continue;
		}
		// (an ink of its own, so that its strength can follow its own size on the screen; his name and the bars from
		// the letters' sheet, in their own colour)
		UMaterialInstanceDynamic* Own = UMaterialInstanceDynamic::Create(TallyInk, this);
		if (Section > 0)
		{
			if (!Own)
			{
				Made.TallySize[Section] = 0.0;
				continue;
			}
			Own->SetTextureParameterValue(TEXT("Sheet"), NameSheet);
			Own->SetVectorParameterValue(TEXT("Tin"), TinOf(Section == 1 ? Ink : AirTally::BarWhite));
		}
		Tally->CreateMeshSection(Section, Part.Corners3, Part.Triangles, Part.Normals, Part.Places, Part.Colours, TArray<FProcMeshTangent>(), false);
		Tally->SetMaterial(Section, Own ? Own : TallyInk.Get());
	}
	Made.Tally = Tally;
	if (Parts[0].Triangles.Num() > 0)
	{
		UE_LOG(LogTheater, Display, TEXT("Air battle: aces: %s's %d victories painted on %s (key %d), %d marks in the %s set."),
			UTF8_TO_TCHAR(Ace.AceName.c_str()), Ace.AceKills, *Made.Model, Key, Parts[0].Triangles.Num() / 6, Set);
	}
	if (Parts[1].Triangles.Num() > 0)
	{
		UE_LOG(LogTheater, Display, TEXT("Air battle: aces: \"%s\" painted under %s's cockpit (key %d), capitals %.1f cm, %s ink."),
			*Painted, *Made.Model, Key, PaintedCap * 100.0, Ink == 0x1A1A1Au ? TEXT("black") : Ink == 0xD9B43Au ? TEXT("yellow") : TEXT("off-white"));
	}
	if (Parts[2].Triangles.Num() > 0)
	{
		UE_LOG(LogTheater, Display, TEXT("Air battle: aces: %s's %d victories painted as %d bars on %s's rudder (key %d)."),
			UTF8_TO_TCHAR(Ace.AceName.c_str()), Ace.AceKills, BarsPainted, *Made.Model, Key);
	}
}

void AAirBattle::DrawTally(const FVector& EyeAt)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(AirBattle_DrawTally);
	static const bool bOn = []
	{
		int32 On = 1;
		FParse::Value(FCommandLine::Get(), TEXT("TheaterAces="), On);
		UE_LOG(LogTheater, Display, TEXT("Air battle: aces' victories painted on their aircraft: %s."), On != 0 ? TEXT("on") : TEXT("off"));
		return On != 0;
	}();
	if (!bOn || !State)
	{
		return;
	}
	FAirBattleState& S = *State;
	const std::map<int, AirShow::Group>& Groups = S.Show.Groups();
	// The test ace's group: the first fighter group of its side (any of its groups if it has no fighters).
	const FTestAce& Test = TestAce();
	int32 TestGroup = INDEX_NONE;
	if (Test.Side >= 0)
	{
		for (const auto& [Id, G] : Groups)
		{
			if (G.Side == Test.Side && (G.bFighter || TestGroup == INDEX_NONE))
			{
				TestGroup = Id;
				if (G.bFighter)
				{
					break;
				}
			}
		}
	}
	auto HasAce = [&Groups, TestGroup](int32 Id)
	{
		const auto Found = Groups.find(Id);
		return Id == TestGroup || (Found != Groups.end() && Found->second.AceKills > 0);
	};
	// Each ace's lead: his group's lowest key (its first aircraft, made first), kept once seen, so that when he falls
	// his marks go with him and are not painted on the next man.
	for (const AirShow::PlaneOut& P : S.Show.Planes())
	{
		if (HasAce(P.Group))
		{
			int32& Lead = AceLead.FindOrAdd(P.Group, P.Key);
			Lead = FMath::Min(Lead, P.Key);
		}
	}
	const int32 Ridden = S.bClose ? S.Subject.Plane : INDEX_NONE;
	const double U = UnitsPerMetre;
	// How many pixels a mark is across, from its size, its distance, the lens and the screen: the art director, 18:31Z
	// 22 November, "let a mark fade into the paint once it falls under about 4 px, rather than shimmer or turn into a
	// grey block". Full strength from 7 pixels, gone at 4.
	const double Lens = Eye && Eye->GetCameraComponent() ? FMath::Clamp(static_cast<double>(Eye->GetCameraComponent()->FieldOfView), 5.0, 120.0) : 55.0;
	double ViewW = 1600.0;
	if (GEngine && GEngine->GameViewport)
	{
		FVector2D Size;
		GEngine->GameViewport->GetViewportSize(Size);
		ViewW = FMath::Max(1.0, Size.X);
	}
	const double PixelsPerMetreAt1 = ViewW * 0.5 / FMath::Tan(FMath::DegreesToRadians(Lens * 0.5));
	for (const TPair<int32, int32>& Lead : AceLead)
	{
		FPlaneMade* Made = PlaneMade.Find(Lead.Value);
		const TObjectPtr<UProceduralMeshComponent>* Have = Planes.Find(Lead.Value);
		UProceduralMeshComponent* Mesh = Have ? Have->Get() : nullptr;
		if (!Made)
		{
			continue;
		}
		const auto Found = Groups.find(Lead.Key);
		const double Away = Mesh ? FVector::Dist(Mesh->GetComponentLocation(), EyeAt) / U : 1e9;
		// (on its own mesh, its near shape drawn, near the eye: past a few hundred metres a 9 cm mark is under a pixel)
		const bool bOnShape = !Made->bTwoShapes || Made->bNear;
		// (each part by its own size: a kill mark across, his name's capitals, a rudder bar's height; before it is made,
		// as its model gives them)
		double Sizes[3] = { Made->TallySize[0], Made->TallySize[1], Made->TallySize[2] };
		if (!Made->bTallyTried)
		{
			const FAirTallySide* Side = TallyOfShape(Made->Shape);
			Sizes[0] = Side && Side->Radius > 0.0 ? 2.0 * Side->Radius / U : 0.1;
			Sizes[1] = Side && Side->Line.Num() >= 2 ? Side->Cap * Side->Scale / U : 0.0;
			Sizes[2] = Side && Side->Bars.Num() > 0 ? 2.0 * Side->BarRadius / U : 0.0;
		}
		auto StrengthOf = [PixelsPerMetreAt1, Away](double Size)
		{
			return Size > 0.0 ? FMath::Clamp((Size * PixelsPerMetreAt1 / FMath::Max(Away, 0.1) - 4.0) / 3.0, 0.0, 1.0) : 0.0;
		};
		const double Strength = FMath::Max3(StrengthOf(Sizes[0]), StrengthOf(Sizes[1]), StrengthOf(Sizes[2]));
		const bool bNear = Lead.Value == Ridden || Away < TallyNearM;
		const bool bShow = Found != Groups.end() && Mesh && Mesh->IsVisible() && bOnShape && bNear && Strength > 0.0;
		if (bShow && !Made->bTallyTried)
		{
			AirShow::Group Ace = Found->second;
			if (Lead.Key == TestGroup && Ace.AceKills <= 0)
			{
				Ace.AceName = Test.Name.empty() ? AirTally::SampleTitle(Ace.Nation) : Test.Name;
				Ace.AceKills = Test.Kills;
				Ace.AceBy = Test.By;
			}
			// (no victims recorded: his kills are the other side's)
			if (Ace.AceBy.empty())
			{
				const std::string& Other = Ace.Side == 0 ? S.Board.DefNat : S.Board.AttNat;
				if (!Other.empty())
				{
					Ace.AceBy[Other] = Ace.AceKills;
				}
			}
			MakeTally(Lead.Value, *Made, Mesh, Ace, UTF8_TO_TCHAR(Ace.Nation.c_str()));
		}
		if (UProceduralMeshComponent* Tally = Made->Tally.Get())
		{
			if (Tally->IsVisible() != bShow)
			{
				Tally->SetVisibility(bShow);
			}
			if (bShow)
			{
				for (int32 Section = 0; Section < 3; ++Section)
				{
					if (Made->TallySize[Section] <= 0.0)
					{
						continue;
					}
					if (UMaterialInstanceDynamic* Own = Cast<UMaterialInstanceDynamic>(Tally->GetMaterial(Section)))
					{
						Own->SetScalarParameterValue(TEXT("Strength"), static_cast<float>(StrengthOf(Made->TallySize[Section])));
					}
				}
			}
		}
	}
}

bool AAirBattle::AceMarksAt(int32 Key, FVector& At, FVector& Out)
{
	const FPlaneMade* Made = PlaneMade.Find(Key);
	const TObjectPtr<UProceduralMeshComponent>* Have = Planes.Find(Key);
	const UProceduralMeshComponent* Mesh = Have ? Have->Get() : nullptr;
	const FAirTallySide* Side = Made && Mesh ? TallyOfShape(Made->Shape) : nullptr;
	if (!Side)
	{
		return false;
	}
	// The middle of what he wears: his name, and his first row of marks (slots run from the front of the lowest row)
	// or, a German pilot's, his first bars on the rudder.
	FVector Sum = FVector::ZeroVector, Facing = FVector::ZeroVector;
	int32 Parts = 0;
	auto Take = [&Sum, &Facing, &Parts](const TArray<FVector>& Points, const TArray<FVector>& Ways, int32 Most)
	{
		const int32 N = FMath::Min3(Points.Num(), Ways.Num(), Most);
		if (N <= 0)
		{
			return;
		}
		FVector Here = FVector::ZeroVector, Way = FVector::ZeroVector;
		for (int32 K = 0; K < N; ++K)
		{
			Here += Points[K];
			Way += Ways[K];
		}
		Sum += Here / N;
		Facing += Way.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
		++Parts;
	};
	Take(Side->Line, Side->LineOut, Side->Line.Num());
	if (Made->TallySize[2] > 0.0)
	{
		Take(Side->Bars, Side->BarsOut, AirTally::PerRow);
	}
	else
	{
		Take(Side->At, Side->Out, Side->PerRow);
	}
	if (Parts == 0)
	{
		return false;
	}
	const FTransform& Frame = Mesh->GetComponentTransform();
	At = Frame.TransformPosition(Sum / Parts);
	Out = Frame.TransformVectorNoScale(Facing.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector));
	return true;
}
