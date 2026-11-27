// The bombers' nose art (release 2; Brian, 25 November 2026 01:30Z: "american only"): each American bomber's painting on
// her nose, one of the modeller's 200 (battle-models/air/noseart, the air thread's list names.json; packed for the game by
// Tools/pack_nose_art.py into Content/Data/NoseArt, a quarter of the modeller's size and never otherwise altered), laid on
// the panel the tool lays on her model's own skin (panels.json: the B-17's, port, and starboard too for the few worn on
// both sides), drawn with M_TallyMark over the paint under it as an ace's victories are (AirBattleTally.cpp), while she is
// near the eye on her own mesh. Which painting each aircraft wears is Battle/AirNoseArt's: her seat's, where the battle
// names her group's corps and the board's place.bombers its seats; else dealt from the battle's seed, about one in five a
// bare nose; and once the battle names her as she falls or falls out of her box, that bomber's (painted again here).
// Shown, never counted: nothing in the battle reads it back.
//
// Off unless the Battle rules card's "Bomber nose art" is on (FLandLive's rule "noseArt", off until the Mac holds 45 fps
// with it and the art director has passed it): read once a battle, at its first frame drawn; off, DrawNoseArt returns
// there, and no other call of the scene is made or changed (PlaneMesh takes down a nose art only where one was made).

#include "Battle/AirBattle.h"

#include "Battle/AirBattleState.h"
#include "Battle/AirNoseArt.h"
#include "Battle/AirShow.h"
#include "Battle/LandLive.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Campaign/CampaignTextures.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "ImageUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "ProceduralMeshComponent.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "TheaterCommand.h"

#include <functional>
#include <map>
#include <string>
#include <utility>

namespace
{
	/** How big what a painting shows is, for its strength on the screen (the art director, 18:31Z 22 November: "let a
	 * mark fade into the paint once it falls under about 4 px, rather than shimmer or turn into a grey block"): the
	 * picture's height, metres. Full strength to some 300 m on a 1600-pixel screen, the near shape's own reach. */
	constexpr double NoseArtSizeM = 1.35;
	/** The paintings' lines written to the log, at most (the Mac's check reads which aircraft wears which). */
	constexpr int32 NoseArtMostLogged = 12;

	/** A model's panel (by its file's name), read from panels.json the first time it is asked for; null where it has none. */
	const AirNoseArt::Panel* PanelOf(FAirBattleState& S, const std::string& Kind)
	{
		auto Found = S.NoseArtPanels.find(Kind);
		if (Found == S.NoseArtPanels.end())
		{
			AirNoseArt::Panel Read;
			std::string Why;
			if (!AirNoseArt::ReadPanel(S.NoseArtPanelsText, Kind, Read, Why))
			{
				// (the finer B-17's panel is in a file of its own, read only while the Battle rules card's Finer B-17 is
				// on: off, this text is empty and nothing is read from it)
				if (S.NoseArtFinePanelsText.empty() || !AirNoseArt::ReadPanel(S.NoseArtFinePanelsText, Kind, Read, Why))
				{
					Read = AirNoseArt::Panel();
				}
			}
			Found = S.NoseArtPanels.emplace(Kind, std::move(Read)).first;
		}
		return Found->second.Cols > 1 && Found->second.Rows > 1 ? &Found->second : nullptr;
	}

	FColor ColourOf(unsigned Rgb)
	{
		return FColor(static_cast<uint8>((Rgb >> 16) & 0xFF), static_cast<uint8>((Rgb >> 8) & 0xFF), static_cast<uint8>(Rgb & 0xFF), 255);
	}
}

bool AAirBattle::NoseArtReady()
{
	if (bNoseArtTried)
	{
		return NoseArtInk != nullptr;
	}
	bNoseArtTried = true;
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Campaign/Materials/M_TallyMark.M_TallyMark"));
	if (!Base)
	{
		UE_LOG(LogTheater, Warning, TEXT("Air battle: nose art: no M_TallyMark (Tools/build_content.sh makes it), so no bomber's nose art is painted."));
		return false;
	}
	NoseArtBase = Base;
	NoseArtInk = UMaterialInstanceDynamic::Create(Base, this);
	NoseArtInk->SetVectorParameterValue(TEXT("Tin"), FLinearColor::White);
	NoseArtInk->SetScalarParameterValue(TEXT("Roughness"), PaintRoughness);
	NoseArtInk->SetScalarParameterValue(TEXT("Specular"), PaintSpecular);
	return true;
}

UTexture2D* AAirBattle::NoseArtPicture(int32 Number)
{
	if (const TObjectPtr<UTexture2D>* Have = NoseArtPictures.Find(Number))
	{
		return Have->Get();
	}
	UTexture2D* Texture = nullptr;
	const AirNoseArt::Entry* Entry = State ? State->NoseArtList.Of(Number) : nullptr;
	if (Entry && Entry->bPainted && !Entry->File.empty())
	{
		const FString Path = FPaths::ProjectContentDir() / TEXT("Data") / TEXT("NoseArt") / FString(UTF8_TO_TCHAR(Entry->File.c_str()));
		FImage Image;
		if (FPaths::FileExists(Path) && FImageUtils::LoadImage(*Path, Image) && Image.SizeX > 0 && Image.SizeY > 0)
		{
			// (its clear pixels hold her olive, the tool's plain average of the modeller's own: the smaller copies, the
			// plain average of each four, draw no dark rim round a painting seen from farther off)
			FImage Pixels;
			Image.CopyTo(Pixels, ERawImageFormat::BGRA8, EGammaSpace::sRGB);
			const TArrayView64<FColor> View = Pixels.AsBGRA8();
			TArray<FColor> Colours;
			Colours.Append(View.GetData(), static_cast<int32>(View.Num()));
			Texture = CampaignTextures::Create(Pixels.SizeX, Pixels.SizeY, Colours, true, false, NAME_None);
		}
	}
	if (!Texture)
	{
		UE_LOG(LogTheater, Warning, TEXT("Air battle: nose art: painting %d could not be read (Content/Data/NoseArt), so its bombers fly bare."), Number);
	}
	// (one that cannot be read is kept too, so that it is not looked for again every frame)
	NoseArtPictures.Add(Number, Texture);
	return Texture;
}

bool AAirBattle::MakeNoseArt(int32 Key, FPlaneMade& Made, UProceduralMeshComponent* Mesh, int32 Number)
{
	Made.bNoseArtTried = true;
	if (!State || !Mesh)
	{
		return false;
	}
	FAirBattleState& S = *State;
	FString Kind;
	FVector Middle = FVector::ZeroVector;
	double Fit = 1.0;
	const AirNoseArt::Panel* Panel = ShapeFrameOf(Made.Shape, Kind, Middle, Fit) ? PanelOf(S, TCHAR_TO_UTF8(*Kind)) : nullptr;
	if (!Panel)
	{
		return false;
	}
	const bool bRead = !NoseArtPictures.Contains(Number);
	UTexture2D* Picture = NoseArtPicture(Number);
	if (!Picture)
	{
		return bRead;
	}
	const AirNoseArt::Entry* Entry = S.NoseArtList.Of(Number);
	const bool bBoth = AirNoseArt::BothSides(Number);
	UProceduralMeshComponent* Art = NewObject<UProceduralMeshComponent>(this, MakeUniqueObjectName(this, UProceduralMeshComponent::StaticClass(), *FString::Printf(TEXT("AirNoseArt_%d"), Key)));
	Art->SetupAttachment(Mesh);
	Art->SetMobility(EComponentMobility::Movable);
	Art->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Art->SetCastShadow(false);
	// (lit as her paint is, by the light thrown up from under the eye too)
	Art->LightingChannels.bChannel1 = true;
	Art->bUseAsyncCooking = false;
	Art->RegisterComponent();
	const double U = UnitsPerMetre;
	// (the paint it lies on: what it fades into when it is small on the screen)
	const FColor Under = ColourOf(Panel->Skin);
	int32 Section = 0;
	for (const AirNoseArt::Side* Side : { &Panel->Port, &Panel->Starboard })
	{
		if (Side == &Panel->Starboard && !bBoth)
		{
			continue;
		}
		// The panel's points as her shape's were brought in from the same file (AirParts::TakeTally's slots): the file
		// has x forward and y to the right, the shape her nose toward -X and her right hand at -Y, in engine units about
		// its middle. Its triangles already face out as the engine draws a face (Battle/AirNoseArt), and a turn of the
		// frame keeps them so.
		TArray<FVector> Corners, Normals;
		TArray<FVector2D> Places;
		TArray<FColor> Colours;
		TArray<int32> Triangles;
		Corners.Reserve(static_cast<int32>(Side->Points.size()));
		for (const AirNoseArt::Point& Point : Side->Points)
		{
			Corners.Add((FVector(-Point.At[0], -Point.At[1], Point.At[2]) * U - Middle) * Fit);
			Normals.Add(FVector(-Point.Facing[0], -Point.Facing[1], Point.Facing[2]).GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector));
			Places.Add(FVector2D(Point.U, Point.V));
			Colours.Add(Under);
		}
		Triangles.Reserve(static_cast<int32>(Side->Triangles.size()));
		for (const int Corner : Side->Triangles)
		{
			Triangles.Add(Corner);
		}
		// (an ink of its own: its painting, and its strength following her size on the screen. Made from M_TallyMark
		// itself with the shared ink's settings, not as a child of the shared ink: on the Mac's P48 stills every
		// painting showed as the material's own grey chequer, its picture never reaching the paint, and a painting's ink
		// one step from the material is the plain way an ink takes its picture)
		UMaterialInstanceDynamic* Own = UMaterialInstanceDynamic::Create(NoseArtBase ? NoseArtBase.Get() : static_cast<UMaterialInterface*>(NoseArtInk.Get()), this);
		if (!Own)
		{
			continue;
		}
		Own->SetVectorParameterValue(TEXT("Tin"), FLinearColor::White);
		Own->SetScalarParameterValue(TEXT("Roughness"), PaintRoughness);
		Own->SetScalarParameterValue(TEXT("Specular"), PaintSpecular);
		Own->SetTextureParameterValue(TEXT("Sheet"), Picture);
		if (!bNoseArtChecked)
		{
			// (once a battle, the first painting laid: what the Mac's log must show for a painting to draw. A picture
			// with no levels or not on the draw thread, an ink whose Sheet reads back as anything but the picture, or
			// M_TallyMark with no Sheet at all, each names where the chequer comes from)
			bNoseArtChecked = true;
			// (the shared ink was made from M_TallyMark and never given a Sheet, so it reads back the material's own: the
			// engine's grey chequer when M_TallyMark has a Sheet, nothing when it has none)
			UTexture* Read = Own->K2_GetTextureParameterValue(TEXT("Sheet"));
			UTexture* Default = NoseArtInk ? NoseArtInk->K2_GetTextureParameterValue(TEXT("Sheet")) : nullptr;
			const FTexturePlatformData* Data = Picture->GetPlatformData();
			UE_LOG(LogTheater, Display, TEXT("Air battle: nose art check: painting %d is %dx%d, %d levels, %s; its ink's Sheet %s; M_TallyMark %s."),
				Number, Picture->GetSizeX(), Picture->GetSizeY(), Data ? Data->Mips.Num() : 0,
				Picture->GetResource() ? TEXT("on the draw thread") : TEXT("NOT on the draw thread"),
				Read == Picture ? TEXT("is the painting") : Read ? *FString::Printf(TEXT("is %s, not the painting"), *Read->GetName()) : TEXT("reads nothing"),
				Default ? *FString::Printf(TEXT("has a Sheet, %s when none is given"), *Default->GetName()) : TEXT("reads no Sheet (rebuild it: TC_REBUILD=M_TallyMark Tools/build_content.sh)"));
		}
		Art->CreateMeshSection(Section, Corners, Triangles, Normals, Places, Colours, TArray<FProcMeshTangent>(), false);
		Art->SetMaterial(Section, Own);
		++Section;
	}
	Made.NoseArt = Art;
	if (NoseArtLogged < NoseArtMostLogged)
	{
		++NoseArtLogged;
		const FString Called = Entry ? FString(UTF8_TO_TCHAR(Entry->Name.c_str())) : FString(TEXT("?"));
		UE_LOG(LogTheater, Display, TEXT("Air battle: nose art: \"%s\" (painting %d) on %s (key %d), %s."),
			*Called, Number, *Made.Model, Key, bBoth ? TEXT("both sides") : TEXT("her port side"));
	}
	return bRead;
}

bool AAirBattle::NoseArtEye(int32 Key, FVector& At, FVector& Out)
{
	static const bool bAsked = FParse::Param(FCommandLine::Get(), TEXT("TheaterAirNoseArtEye"));
	if (!bAsked || !State || State->NoseArtOn != 1 || State->NoseArt.ArtOf(Key) <= 0)
	{
		return false;
	}
	const FPlaneMade* Made = PlaneMade.Find(Key);
	const TObjectPtr<UProceduralMeshComponent>* Have = Planes.Find(Key);
	const UProceduralMeshComponent* Mesh = Have ? Have->Get() : nullptr;
	FString Kind;
	FVector Middle = FVector::ZeroVector;
	double Fit = 1.0;
	const AirNoseArt::Panel* Panel = Made && Mesh && ShapeFrameOf(Made->Shape, Kind, Middle, Fit) ? PanelOf(*State, TCHAR_TO_UTF8(*Kind)) : nullptr;
	if (!Panel)
	{
		return false;
	}
	// (the port panel's middle and facing, brought in as her points were: AirParts::TakeTally's slots)
	const AirNoseArt::Side& Port = Panel->Port;
	const FVector Local = (FVector(-Port.Middle[0], -Port.Middle[1], Port.Middle[2]) * UnitsPerMetre - Middle) * Fit;
	const FVector Facing = FVector(-Port.Normal[0], -Port.Normal[1], Port.Normal[2]).GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
	const FTransform& Frame = Mesh->GetComponentTransform();
	At = Frame.TransformPosition(Local);
	Out = Frame.TransformVectorNoScale(Facing);
	return true;
}

void AAirBattle::DrawNoseArt(const FVector& EyeAt)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(AirBattle_DrawNoseArt);
	if (!State)
	{
		return;
	}
	FAirBattleState& S = *State;
	if (S.NoseArtOn < 0)
	{
		S.NoseArtOn = FLandLive::RuleNow(TEXT("noseArt")) ? 1 : 0;
		// (said only when on: off, the battle's log is as it was before there was nose art)
		if (S.NoseArtOn)
		{
			UE_LOG(LogTheater, Display, TEXT("Air battle: bombers' nose art: on (the Battle rules card's Bomber nose art, or -TheaterAirNoseArt)."));
		}
	}
	if (S.NoseArtOn == 0)
	{
		return;
	}
	// The list and the panels, read once a battle; its paintings then told from the battle's seed and its board's seats.
	if (!S.bNoseArtRead)
	{
		S.bNoseArtRead = true;
		// (no M_TallyMark: NoseArtReady has said so in the log, and every bomber flies bare)
		if (!NoseArtReady())
		{
			S.NoseArtOn = 0;
			return;
		}
		// Each file its own step, so that the log names the one at fault (the review's UE-1, 25 November).
		const FString Folder = FPaths::ProjectContentDir() / TEXT("Data") / TEXT("NoseArt");
		TArray<uint8> Index, Panels;
		std::string Why;
		bool bRead = false;
		if (!FFileHelper::LoadFileToArray(Index, *(Folder / TEXT("index.json"))))
		{
			Why = "index.json not read";
		}
		else if (!AirNoseArt::ReadIndex(std::string(reinterpret_cast<const char*>(Index.GetData()), static_cast<size_t>(Index.Num())), S.NoseArtList, Why))
		{
			Why = "index.json: " + Why;
		}
		else if (!FFileHelper::LoadFileToArray(Panels, *(Folder / TEXT("panels.json"))))
		{
			Why = "panels.json not read";
		}
		else
		{
			bRead = true;
		}
		if (!bRead)
		{
			UE_LOG(LogTheater, Warning, TEXT("Air battle: nose art: its list or panels could not be read (Content/Data/NoseArt: %s), so every bomber flies bare."), UTF8_TO_TCHAR(Why.c_str()));
			S.NoseArtOn = 0;
			return;
		}
		S.NoseArtPanelsText.assign(reinterpret_cast<const char*>(Panels.GetData()), static_cast<size_t>(Panels.Num()));
		// (the finer B-17's own panel, laid on her newer skin by Tools/pack_nose_art.py --fine: only while she flies; not
		// read, she flies bare and the rest wear theirs)
		if (S.B17FineOn == 1)
		{
			TArray<uint8> FinePanels;
			if (FFileHelper::LoadFileToArray(FinePanels, *(Folder / TEXT("panels-fine.json"))))
			{
				S.NoseArtFinePanelsText.assign(reinterpret_cast<const char*>(FinePanels.GetData()), static_cast<size_t>(FinePanels.Num()));
			}
			else
			{
				UE_LOG(LogTheater, Warning, TEXT("Air battle: nose art: Content/Data/NoseArt/panels-fine.json could not be read, so the finer B-17s fly bare."));
			}
		}
		S.NoseArt.Begin(S.NoseArtList, S.SkyPlace.Seed, S.Board.Bombers);
		UE_LOG(LogTheater, Display, TEXT("Air battle: nose art: %d paintings; the board names the seats of %d bomber groups."),
			static_cast<int32>(S.NoseArtList.Painted().size()), static_cast<int32>(S.Board.Bombers.size()));
	}
	// (whether a model the battle names has a panel: by the shape it is drawn as, AAirBattle::ModelFor; a stand-in has none)
	auto HasPanel = [this, &S](const std::string& Asked)
	{
		const auto Known = S.NoseArtModels.find(Asked);
		if (Known != S.NoseArtModels.end())
		{
			return Known->second;
		}
		const FString Name = ModelFor(UTF8_TO_TCHAR(Asked.c_str()), TEXT("USA"), false);
		FString Kind, Whose;
		const bool bSculpted = !Name.StartsWith(TEXT("Fighter:")) && !Name.StartsWith(TEXT("Bomber:")) && Name.Split(TEXT(":"), &Kind, &Whose);
		const bool bHas = bSculpted && PanelOf(S, TCHAR_TO_UTF8(*Kind)) != nullptr;
		S.NoseArtModels[Asked] = bHas;
		return bHas;
	};
	const int32 GivenBefore = S.NoseArt.Given();
	S.NoseArt.Take(S.Show.Groups(), S.Show.Planes(), HasPanel);
	if (S.NoseArt.Given() > GivenBefore && GivenBefore == 0)
	{
		for (const auto& Pair : S.Show.Groups())
		{
			const AirShow::Group& G = Pair.second;
			if (AirNoseArt::Wears(G.Nation, G.bFighter, G.Model, HasPanel))
			{
				UE_LOG(LogTheater, Display, TEXT("Air battle: nose art: %s's paintings %s."), UTF8_TO_TCHAR(G.Name.c_str()),
					S.NoseArt.HowTold(G.Id) == AirNoseArt::Told::ByCorps ? TEXT("from its seats (its corps named)") : TEXT("dealt from the battle's seed"));
			}
		}
	}
	// A bomber the battle named as she fell, or fell out of her box, wears that bomber's painting from then, and the one
	// that wore it hers (Battle/AirNoseArt): each painted again, her old nose art taken down and made afresh below when
	// she is next shown (none where she is now bare).
	for (const int32 Key : S.NoseArt.Repainted())
	{
		FPlaneMade* Made = PlaneMade.Find(Key);
		if (!Made)
		{
			continue;
		}
		if (Made->NoseArt.IsValid())
		{
			Made->NoseArt->DestroyComponent();
		}
		Made->NoseArt.Reset();
		Made->bNoseArtTried = false;
		if (NoseArtLogged < NoseArtMostLogged)
		{
			++NoseArtLogged;
			const int32 Number = S.NoseArt.ArtOf(Key);
			const AirNoseArt::Entry* Entry = Number > 0 ? S.NoseArtList.Of(Number) : nullptr;
			const FString Called = Entry ? FString(UTF8_TO_TCHAR(Entry->Name.c_str())) : FString(TEXT("a bare nose"));
			UE_LOG(LogTheater, Display, TEXT("Air battle: nose art: key %d now wears \"%s\" (painting %d): the battle named a bomber as one fell, or fell out of her box."),
				Key, *Called, Number);
		}
	}
	const int32 Ridden = S.bClose ? S.Subject.Plane : INDEX_NONE;
	const double U = UnitsPerMetre;
	// How many pixels the painting is high, from the lens and the screen, as an ace's marks are (AirBattleTally.cpp).
	const double Lens = Eye && Eye->GetCameraComponent() ? FMath::Clamp(static_cast<double>(Eye->GetCameraComponent()->FieldOfView), 5.0, 120.0) : 55.0;
	double ViewW = 1600.0;
	if (GEngine && GEngine->GameViewport)
	{
		FVector2D Size;
		GEngine->GameViewport->GetViewportSize(Size);
		ViewW = FMath::Max(1.0, Size.X);
	}
	const double PixelsPerMetreAt1 = ViewW * 0.5 / FMath::Tan(FMath::DegreesToRadians(Lens * 0.5));
	// Whether an aircraft's nose art is shown, and at what strength: on her own mesh, her near shape drawn, near the eye
	// (the ridden one, or within NoseArtNearM), and some 4 pixels high or more.
	auto Shown = [this, Ridden, U, &EyeAt, PixelsPerMetreAt1](int32 Key, const FPlaneMade& Made, double& Strength)
	{
		const TObjectPtr<UProceduralMeshComponent>* Have = Planes.Find(Key);
		const UProceduralMeshComponent* Mesh = Have ? Have->Get() : nullptr;
		if (!Mesh)
		{
			Strength = 0.0;
			return false;
		}
		const double Away = FVector::Dist(Mesh->GetComponentLocation(), EyeAt) / U;
		const bool bOnShape = !Made.bTwoShapes || Made.bNear;
		// (the finer B-17's panel is smaller than the older one's, 0.61 m high against 1.35 (panels-fine.json): faded by its
		// own height, so that it too fades under about 4 px; every other aircraft by NoseArtSizeM as before)
		double HighM = NoseArtSizeM;
		FString Kind;
		FVector Middle = FVector::ZeroVector;
		double Fit = 1.0;
		if (Made.bFine && State.IsValid() && ShapeFrameOf(Made.Shape, Kind, Middle, Fit))
		{
			if (const AirNoseArt::Panel* Own = PanelOf(*State, TCHAR_TO_UTF8(*Kind)))
			{
				HighM = Own->HighM * Fit;
			}
		}
		Strength = FMath::Clamp((HighM * PixelsPerMetreAt1 / FMath::Max(Away, 0.1) - 4.0) / 3.0, 0.0, 1.0);
		return Mesh->IsVisible() && bOnShape && (Key == Ridden || Away < NoseArtNearM) && Strength > 0.0;
	};
	// Made when first shown (at most one painting read a frame: each is a small picture, and twenty near at once would
	// stall a frame) ...
	bool bReadOne = false;
	for (const AirShow::PlaneOut& P : S.Show.Planes())
	{
		const int32 Number = S.NoseArt.ArtOf(P.Key);
		FPlaneMade* Made = Number > 0 ? PlaneMade.Find(P.Key) : nullptr;
		const TObjectPtr<UProceduralMeshComponent>* Have = Made && !Made->bNoseArtTried ? Planes.Find(P.Key) : nullptr;
		UProceduralMeshComponent* Mesh = Have ? Have->Get() : nullptr;
		double Strength = 0.0;
		if (Mesh && (!bReadOne || NoseArtPictures.Contains(Number)) && Shown(P.Key, *Made, Strength))
		{
			bReadOne = MakeNoseArt(P.Key, *Made, Mesh, Number) || bReadOne;
		}
	}
	// ... and every one made shown or hidden with her (one whose aircraft is gone hidden with its mesh), at her strength.
	for (TPair<int32, FPlaneMade>& Pair : PlaneMade)
	{
		UProceduralMeshComponent* Art = Pair.Value.NoseArt.Get();
		if (!Art)
		{
			continue;
		}
		double Strength = 0.0;
		const bool bShow = Shown(Pair.Key, Pair.Value, Strength);
		if (Art->IsVisible() != bShow)
		{
			Art->SetVisibility(bShow);
		}
		if (bShow)
		{
			for (int32 Section = 0; Section < Art->GetNumSections(); ++Section)
			{
				if (UMaterialInstanceDynamic* Own = Cast<UMaterialInstanceDynamic>(Art->GetMaterial(Section)))
				{
					Own->SetScalarParameterValue(TEXT("Strength"), static_cast<float>(Strength));
				}
			}
		}
	}
}
