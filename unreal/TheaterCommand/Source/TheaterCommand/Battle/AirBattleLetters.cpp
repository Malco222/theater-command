// Bomber markings (release 2; the Battle rules card's "Bomber markings", FLandLive's rule "flyBomberMarks"; Brian, 25
// November 2026, of the approved studio pictures of the newer B-17: "Build it into the game"; 26 November: "i want the new
// ones in the game"). Every finer B-17 wears her group's white triangle and its black letter on her fin and on her upper
// starboard wing, ridden, near and far: the mark meshes of Content/Data/AirMarks/b17-fine-marks.json (made by
// Tools/air_marks/make_b17_marks.py from the model thread's livery file, laid on her newer skin), each drawn over her as an
// instanced copy with her own place and her place a frame before (CopiesFor, sent with the aircraft's by SendCopies), on
// PlaneInk: no new material, texture or content build. Which letters she wears is Battle/AirLetters' book, from the show's
// aircraft alone: the same on every run, never by the date, never from a random draw of the battle.
//
// Off unless the row is on (and the finer B-17 flies, and the file reads): read once a battle (LettersOnNow), as its
// shapes are first made ready or its first frame drawn; off, no file is read, nothing is made, placed or said, and the
// scene's other calls are as they were. Looks only: nothing the rules count reads it.

#include "Battle/AirBattle.h"

#include "Battle/AirBattleState.h"
#include "Battle/AirLetters.h"
#include "Battle/AirShow.h"
#include "Battle/LandLive.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "TheaterCommand.h"

#include <string>
#include <vector>

bool AAirBattle::LettersOnNow()
{
	if (!State)
	{
		return false;
	}
	FAirBattleState& S = *State;
	if (S.LettersOn < 0)
	{
		// (the finer B-17 first: the marks are laid on her skin alone, and the older B-17 is never touched)
		S.LettersOn = S.B17FineOn == 1 && FLandLive::RuleNow(TEXT("flyBomberMarks")) ? 1 : 0;
		if (S.LettersOn == 1)
		{
			const double Began = FPlatformTime::Seconds();
			FString Text;
			std::string Why = "Content/Data/AirMarks/b17-fine-marks.json not read";
			if (FFileHelper::LoadFileToString(Text, *(FPaths::ProjectContentDir() / TEXT("Data") / TEXT("AirMarks") / TEXT("b17-fine-marks.json")))
				&& S.LettersSheet.Read(std::string(TCHAR_TO_UTF8(*Text)), Why))
			{
				S.Letters.Begin(S.LettersSheet, S.Show.Groups());
				FParse::Value(FCommandLine::Get(), TEXT("TheaterAirMarksMinPx="), MarksMinPx[0]);
				FParse::Value(FCommandLine::Get(), TEXT("TheaterAirMarksLetterMinPx="), MarksMinPx[1]);
				MarksMinPx[0] = FMath::Clamp(MarksMinPx[0], 0.0, 100.0);
				MarksMinPx[1] = FMath::Clamp(MarksMinPx[1], 0.0, 100.0);
				int32 Lettered = 0;
				for (const auto& Pair : S.Show.Groups())
				{
					Lettered += S.Letters.LetterOf(Pair.first) != 0 ? 1 : 0;
				}
				UE_LOG(LogTheater, Display, TEXT("Air battle: bomber markings: on (the Battle rules card's Bomber markings, or -TheaterAirBomberMarks): bundle %d, %d parts read in %.0f ms; %d groups lettered; drawn down to %.1f px (triangle) and %.1f px (letters)."),
					S.LettersSheet.Bundle, static_cast<int32>(S.LettersSheet.Parts.size()), 1000.0 * (FPlatformTime::Seconds() - Began), Lettered, MarksMinPx[0], MarksMinPx[1]);
			}
			else
			{
				S.LettersOn = 0;
				S.LettersSheet = AirLetters::Sheet();
				UE_LOG(LogTheater, Warning, TEXT("Air battle: bomber markings: on, but %s: the finer B-17s fly with the star alone, as with the row off."), UTF8_TO_TCHAR(Why.c_str()));
			}
		}
	}
	return S.LettersOn == 1;
}

double AAirBattle::MarksPixelsPerMetre(double Lens) const
{
	// (as her nose art reckons it, AirBattleNoseArt.cpp: the screen's width over the lens)
	double ViewW = 1600.0;
	if (GEngine && GEngine->GameViewport)
	{
		FVector2D Size;
		GEngine->GameViewport->GetViewportSize(Size);
		ViewW = FMath::Max(1.0, Size.X);
	}
	return ViewW * 0.5 / FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(Lens, 5.0, 120.0) * 0.5));
}

void AAirBattle::WarmMarks()
{
	if (!State || State->LettersOn != 1)
	{
		return;
	}
	FAirBattleState& S = *State;
	S.Letters.Take(S.Show.Planes(), &S.Show.Groups());
	int32 Made = 0;
	TSet<FString> Done;
	for (const AirShow::PlaneOut& P : S.Show.Planes())
	{
		const AirLetters::Wear* W = S.Letters.Of(P.Key);
		if (!W)
		{
			continue;
		}
		// (her shape as the scene makes it: the same key PlaneMesh asks for, made ready a moment ago by WarmShapes)
		const FString ShapeKey = ShapeFor(FString(UTF8_TO_TCHAR(P.Model.c_str())), FString(UTF8_TO_TCHAR(P.Nation.c_str())), P.bFighter, P.Span, false);
		for (const std::string& Name : AirLetters::MeshesOf(*W, S.LettersSheet.Bundle))
		{
			const FString Mesh(UTF8_TO_TCHAR(Name.c_str()));
			if (Done.Contains(ShapeKey + TEXT("|") + Mesh))
			{
				continue;
			}
			Done.Add(ShapeKey + TEXT("|") + Mesh);
			Made += MarkStill(Mesh, true, ShapeKey) ? 1 : 0;
			Made += MarkStill(Mesh, false, ShapeKey) ? 1 : 0;
		}
	}
	UE_LOG(LogTheater, Display, TEXT("Air battle: bomber markings: %d bombers lettered, %d mark meshes made ready."), S.Letters.Given(), Made);
}

void AAirBattle::AddMarks(const AirShow::PlaneOut& P, FPlaneMade& Made, const FTransform& Now, const FTransform& Was, double AwayM, bool bNearLevel, bool bRidden, double PixelsPerMetreAt1)
{
	const AirLetters::Wear* W = State && State->LettersOn == 1 ? State->Letters.Of(P.Key) : nullptr;
	if (!W)
	{
		Made.MarksShown = 0;
		return;
	}
	const std::vector<std::string> Meshes = AirLetters::MeshesOf(*W, State->LettersSheet.Bundle);
	uint8 Shown = 0;
	for (int32 K = 0; K < static_cast<int32>(Meshes.size()) && K < 8; ++K)
	{
		const std::string& Name = Meshes[static_cast<size_t>(K)];
		const bool bWas = (Made.MarksShown & (1u << K)) != 0;
		const double MinPx = Name.compare(0, 5, "unit.") == 0 ? MarksMinPx[0] : MarksMinPx[1];
		if (!bRidden && !AirLetters::Shown(AirLetters::SizeOf(Name), PixelsPerMetreAt1, AwayM, MinPx, bWas))
		{
			continue;
		}
		const FString Mesh(UTF8_TO_TCHAR(Name.c_str()));
		UStaticMesh* Still = MarkStill(Mesh, bNearLevel, Made.Shape);
		// (one copies mesh a mark mesh, level and shape, as an aircraft's are kept by her shape)
		FPlaneCopies* Group = Still ? CopiesFor(TEXT("marks|") + Mesh + (bNearLevel ? TEXT("|near|") : TEXT("|far|")) + Made.Shape, false, Still, PlaneInk) : nullptr;
		if (!Group)
		{
			continue;
		}
		Group->Keys.Add(P.Key);
		Group->At.Add(Now);
		Group->Before.Add(Was);
		Shown |= static_cast<uint8>(1u << K);
	}
	Made.MarksShown = Shown;
}
