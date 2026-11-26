// The groups' earned colours (release 2; Brian, 25 November 2026 03:27Z: "earned nicknames as they complete missions. i
// also want them to get unique colorings or markings like the tuskegee airmen"): a group the rules say has earned colours
// (the board's place.corpsMarks, by its corps) flies its nation's scheme of the art director's palette
// (Content/Data/Air/unit-colours.json, from art-director/air-colours) as far as it has earned it: the spinner, then the
// band round the nose, then the tail (Battle/AirUnitColours finds where on each model and lays it). Its name is the
// rules' and the cards', never painted on the aircraft. Shown, never counted: nothing in the battle reads it back.
//
// Off unless the Battle rules card's "Earned colours" is on (FLandLive's rule "unitColours", off until the art director
// has scored the stills and the historian's check is through): read once a battle, at its first aircraft made; off,
// UnitColoursShape returns there and the aircraft is made exactly as before. Its shapes are made in
// AirBattleUnitColours.inl (AirBattle.cpp's own), with what is found here.

#include "Battle/AirBattle.h"

#include "Battle/AirBattleState.h"
#include "Battle/AirShow.h"
#include "Battle/AirSkin.h"
#include "Battle/AirUnitColours.h"
#include "Battle/LandLive.h"
#include "Campaign/CampaignUnitTable.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "TheaterCommand.h"

#include <string>
#include <utility>
#include <vector>

namespace
{
	/** The looks written to the log, at most (the Mac's check reads which group wears which). */
	constexpr int32 UnitColoursMostLogged = 16;

	bool ReadText(const FString& Path, std::string& Out)
	{
		TArray<uint8> Bytes;
		if (!FFileHelper::LoadFileToArray(Bytes, *Path))
		{
			return false;
		}
		Out.assign(reinterpret_cast<const char*>(Bytes.GetData()), static_cast<size_t>(Bytes.Num()));
		return true;
	}
}

bool AAirBattle::UnitColoursSwitch(bool& bFirst)
{
	bFirst = false;
	if (!State)
	{
		return false;
	}
	FAirBattleState& S = *State;
	if (S.UnitColoursOn < 0)
	{
		bFirst = true;
		S.UnitColoursOn = FLandLive::RuleNow(TEXT("unitColours")) ? 1 : 0;
		// (said only when on: off, the battle's log is as it was before there were earned colours)
		if (S.UnitColoursOn)
		{
			std::string Text, Why;
			const FString Path = FPaths::ProjectContentDir() / TEXT("Data") / TEXT("Air") / TEXT("unit-colours.json");
			if (!ReadText(Path, Text) || !AirUnitColours::ReadPalette(Text, S.UnitColoursPalette, Why))
			{
				UE_LOG(LogTheater, Warning, TEXT("Air battle: earned colours: the palette (Content/Data/Air/unit-colours.json) does not read (%s), so every aircraft flies in its plain paint."), UTF8_TO_TCHAR(Why.c_str()));
				S.UnitColoursOn = 0;
			}
			else
			{
				UE_LOG(LogTheater, Display, TEXT("Air battle: earned colours: on (the Battle rules card's Earned colours, or -TheaterAirUnitColours); the board names %d groups with colours."),
					static_cast<int32>(S.Board.CorpsMarks.size()));
			}
		}
	}
	return S.UnitColoursOn == 1;
}

bool AAirBattle::UnitColoursFor(int32 Key, const FString& Kind, FString& Look)
{
	if (!State || State->UnitColoursOn != 1 || State->Board.CorpsMarks.empty())
	{
		return false;
	}
	FAirBattleState& S = *State;
	// The aircraft's group, and its corps.
	const AirShow::PlaneOut* Plane = nullptr;
	for (const AirShow::PlaneOut& P : S.Show.Planes())
	{
		if (P.Key == Key)
		{
			Plane = &P;
			break;
		}
	}
	if (!Plane)
	{
		return false;
	}
	const auto Group = S.Show.Groups().find(Plane->Group);
	if (Group == S.Show.Groups().end())
	{
		return false;
	}
	AirUnitColours::Look Wears;
	if (!AirUnitColours::LookOf(true, S.Board.CorpsMarks, Group->second.Corps, S.UnitColoursPalette, S.Board.bNight, S.Board.Month, Wears))
	{
		return false;
	}
	// (a plain machine's rudder stripes, where its nation has any: a patch is laid under them only then)
	bool bUpright = true;
	TArray<FLinearColor> Bands;
	Wears.bFinStripes = ::UnitTable::Fin(UTF8_TO_TCHAR(Plane->Nation.c_str()), bUpright, Bands);
	// Its model, read and its regions found the first time a group wears colours on it.
	const std::string Name(TCHAR_TO_UTF8(*Kind));
	FAirBattleState::FUnitColoursModel& Known = S.UnitColoursModels[Name];
	if (!Known.bRead)
	{
		Known.bRead = true;
		std::string Text, Why;
		const FString Path = FPaths::ProjectContentDir() / TEXT("Models") / (Kind + TEXT(".json"));
		if (!ReadText(Path, Text) || !AirUnitColours::ReadModel(Text, Name, Known.Model, Why))
		{
			UE_LOG(LogTheater, Warning, TEXT("Air battle: earned colours: the model %s does not read (%s), so its groups fly in their plain paint."), *Kind, UTF8_TO_TCHAR(Why.c_str()));
			Known.Model = AirUnitColours::Model();
		}
		else
		{
			// (the finer B-17 under the Battle rules card's "Worn B-17 paint": her earned colours laid over her worn paint,
			// its own stains in their bands; AirSkin holds nothing with the row off)
			if (const AirSkin::Overlay* WornSkin = AirSkin::Active() ? AirSkin::For(Name) : nullptr)
			{
				if (!AirSkin::OntoModel(*WornSkin, Known.Model))
				{
					UE_LOG(LogTheater, Warning, TEXT("Air battle: earned colours: %s's worn paint is not of her counts, so her bands lie on her plain paint."), *Kind);
				}
			}
			Known.Found = AirUnitColours::Find(Known.Model);
			if (!Known.Found.Note.empty())
			{
				UE_LOG(LogTheater, Display, TEXT("Air battle: earned colours: %s: %s"), *Kind, UTF8_TO_TCHAR(Known.Found.Note.c_str()));
			}
		}
	}
	if (Known.Model.Near.Count() == 0)
	{
		return false;
	}
	S.UnitColoursLook = Wears;
	S.UnitColoursKind = Name;
	const std::string Key8 = AirUnitColours::KeyOf(Known.Model, Wears) + (Wears.bFinStripes ? "" : " plain rudder");
	Look = UTF8_TO_TCHAR(Key8.c_str());
	const std::string Logged = Group->second.Name + "|" + Name;
	if (static_cast<int32>(S.UnitColoursLogged.size()) < UnitColoursMostLogged && S.UnitColoursLogged.insert(Logged).second)
	{
		const auto Row = S.Board.CorpsMarks.find(Group->second.Corps);
		const AirUnitColours::Scheme* Scheme = S.UnitColoursPalette.Of(Wears.Nation, Wears.Scheme);
		UE_LOG(LogTheater, Display, TEXT("Air battle: earned colours: %s (corps %d, \"%s\") on its %s, step %d of %s scheme %d (%s)%s%s."), UTF8_TO_TCHAR(Group->second.Name.c_str()),
			Group->second.Corps, Row != S.Board.CorpsMarks.end() ? UTF8_TO_TCHAR(Row->second.Name.c_str()) : TEXT(""), *Kind, Wears.Tier, UTF8_TO_TCHAR(Wears.Nation.c_str()), Wears.Scheme + 1,
			Scheme ? UTF8_TO_TCHAR(Scheme->Words.c_str()) : TEXT(""), AirUnitColours::NightFinish(Known.Model, Wears) ? TEXT(", its night finish") : TEXT(""),
			Wears.bWinter ? TEXT(", a winter month") : TEXT(""));
	}
	return true;
}

bool AAirBattle::UnitColoursLay(bool bFar, const FVector& Middle, double Fit, TArray<FVector>& Points, TArray<int32>& Triangles, TArray<FVector>& Normals, TArray<FColor>& Colors)
{
	if (!State)
	{
		return false;
	}
	FAirBattleState& S = *State;
	const auto Known = S.UnitColoursModels.find(S.UnitColoursKind);
	if (Known == S.UnitColoursModels.end())
	{
		return false;
	}
	const AirUnitColours::Model& M = Known->second.Model;
	const AirUnitColours::Mesh& File = bFar ? M.Far : M.Near;
	const int32 Count = static_cast<int32>(File.Count());
	if (Count == 0 || Points.Num() < Count || Normals.Num() != Points.Num() || Colors.Num() != Points.Num())
	{
		return false;
	}
	// (the shape's points are its file's, in order, brought in as the scene brings them: its nose toward -X and its right
	// hand at -Y, about Middle, by Fit; checked at a few, so that a shape read otherwise is never painted wrong)
	const double U = UnitsPerMetre;
	for (int32 K = 0; K < Count; K += FMath::Max(1, Count / 16))
	{
		const FVector From = (FVector(-File.X(static_cast<size_t>(K)), -File.Y(static_cast<size_t>(K)), File.Z(static_cast<size_t>(K))) * U - Middle) * Fit;
		if (FVector::Dist(From, Points[K]) > 0.01 * U * FMath::Max(Fit, 0.01))
		{
			UE_LOG(LogTheater, Warning, TEXT("Air battle: earned colours: the %s shape of %s is not its file's, so it flies in its plain paint."), bFar ? TEXT("far") : TEXT("near"), UTF8_TO_TCHAR(S.UnitColoursKind.c_str()));
			return false;
		}
	}
	const AirUnitColours::Coat Coat = AirUnitColours::Paint(M, Known->second.Found, bFar, S.UnitColoursPalette, S.UnitColoursLook);
	if (Coat.Points.empty() && Coat.Cuts.empty())
	{
		return false;
	}
	// The pieces' new corners, from the shape's own corners as they were (a kept piece mixes their colours as the triangle
	// it was cut from did) ...
	const TArray<FColor> Was = Colors;
	const std::vector<int> Before(Triangles.GetData(), Triangles.GetData() + Triangles.Num());
	std::vector<int> After;
	std::vector<std::pair<int, int>> Added;
	AirUnitColours::LayTriangles(Coat, Before, Count, Points.Num(), After, Added);
	for (const std::pair<int, int>& Each : Added)
	{
		const AirUnitColours::Corner& Co = Coat.Cuts[static_cast<size_t>(Each.first)].Corners[static_cast<size_t>(Each.second)];
		FVector At = FVector::ZeroVector, Facing = FVector::ZeroVector;
		double Mixed[4] = { 0.0, 0.0, 0.0, 0.0 };
		for (int32 I = 0; I < 3; ++I)
		{
			const int32 From = Co.From[I];
			const double W = Co.Weight[I];
			At += Points[From] * W;
			Facing += Normals[From] * W;
			Mixed[0] += Was[From].R * W;
			Mixed[1] += Was[From].G * W;
			Mixed[2] += Was[From].B * W;
			Mixed[3] += Was[From].A * W;
		}
		auto Byte = [](double V) { return static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(V), 0, 255)); };
		Points.Add(At);
		Normals.Add(Facing.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector));
		Colors.Add(Co.bEarned ? FColor(Co.Colour.R, Co.Colour.G, Co.Colour.B, Byte(Mixed[3])) : FColor(Byte(Mixed[0]), Byte(Mixed[1]), Byte(Mixed[2]), Byte(Mixed[3])));
	}
	// ... and the earned points recoloured, each keeping its finish (the alpha: how worn its paint is).
	for (const std::pair<int, AirUnitColours::Rgb>& Each : Coat.Points)
	{
		FColor& C = Colors[Each.first];
		C = FColor(Each.second.R, Each.second.G, Each.second.B, C.A);
	}
	Triangles.Reset(static_cast<int32>(After.size()));
	for (const int Corner : After)
	{
		Triangles.Add(Corner);
	}
	return true;
}
