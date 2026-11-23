#include "Campaign/CampaignStepPlayer.h"
#include "Campaign/CampaignCameraPawn.h"
#include "Campaign/CampaignMapActor.h"
#include "Campaign/CampaignPlayerController.h"
#include "Campaign/CampaignSession.h"
#include "Campaign/TheaterSpeed.h"
#include "Battle/AirBattle.h"
#include "Battle/AirBattleState.h"
#include "TheaterCommand.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "Input/HittestGrid.h"
#include "Layout/WidgetPath.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RenderTimer.h"
#include "RHI.h"
#include "UnrealClient.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	/** A string as JSON writes it. */
	FString Quote(const FString& Text)
	{
		FString Out = TEXT("\"");
		for (const TCHAR Each : Text)
		{
			if (Each == TEXT('"')) { Out += TEXT("\\\""); }
			else if (Each == TEXT('\\')) { Out += TEXT("\\\\"); }
			else if (Each == TEXT('\n')) { Out += TEXT("\\n"); }
			else if (Each == TEXT('\r')) { }
			else if (Each == TEXT('\t')) { Out += TEXT(" "); }
			else if (Each < 32) { Out += TEXT(" "); }
			else { Out.AppendChar(Each); }
		}
		return Out + TEXT("\"");
	}

	/** Words as they are compared: capitals, single spaces, no dots between. */
	FString Plain(const FString& Text)
	{
		FString Out = Text.ToUpper().Replace(TEXT("·"), TEXT(" ")).Replace(TEXT("\n"), TEXT(" "));
		while (Out.ReplaceInline(TEXT("  "), TEXT(" ")) > 0) { }
		return Out.TrimStartAndEnd();
	}

	/** The air battle he flies himself, where one is up: it stands in the map's world while it is (FlyLive spawns it there). */
	AAirBattle* OpenAirBattle(const ACampaignPlayerController& Player)
	{
		if (UWorld* World = Player.GetWorld())
		{
			for (TActorIterator<AAirBattle> It(World); It; ++It)
			{
				if (IsValid(*It) && It->GetState())
				{
					return *It;
				}
			}
		}
		return nullptr;
	}

	struct FFoundButton
	{
		TSharedPtr<SWidget> Widget;
		FString Words;
		bool bEnabled = true;
	};

	void Lettering(const TSharedRef<SWidget>& Widget, FString& Out)
	{
		static const FName TextType(TEXT("STextBlock"));
		if (!Widget->GetVisibility().IsVisible())
		{
			return;
		}
		if (Widget->GetType() == TextType)
		{
			const FString Words = StaticCastSharedRef<STextBlock>(Widget)->GetText().ToString();
			if (!Words.IsEmpty())
			{
				Out += (Out.IsEmpty() ? TEXT("") : TEXT(" ")) + Words;
			}
			return;
		}
		if (FChildren* Children = Widget->GetChildren())
		{
			for (int32 Index = 0; Index < Children->Num(); ++Index)
			{
				Lettering(Children->GetChildAt(Index), Out);
			}
		}
	}

	/** Every button on the screen that can be seen, with the words on it. */
	void Gather(const TSharedRef<SWidget>& Widget, bool bEnabledAbove, TArray<FFoundButton>& Out)
	{
		static const FName ButtonType(TEXT("SButton"));
		if (!Widget->GetVisibility().IsVisible())
		{
			return;
		}
		const bool bEnabled = bEnabledAbove && Widget->IsEnabled();
		if (Widget->GetType() == ButtonType)
		{
			FFoundButton Found;
			Lettering(Widget, Found.Words);
			const FVector2D Size = Widget->GetTickSpaceGeometry().GetLocalSize();
			if (!Found.Words.IsEmpty() && Size.X > 1.0 && Size.Y > 1.0)
			{
				Found.Widget = Widget;
				Found.bEnabled = bEnabled;
				Out.Add(Found);
			}
			return;
		}
		if (FChildren* Children = Widget->GetChildren())
		{
			for (int32 Index = 0; Index < Children->Num(); ++Index)
			{
				Gather(Children->GetChildAt(Index), bEnabled, Out);
			}
		}
	}

	TArray<FFoundButton> ButtonsOnScreen()
	{
		THEATER_SPEED_SCOPE(HarnessFindButton); // (the step player's own work, which a player does not pay for)
		TArray<FFoundButton> Out;
		const TSharedPtr<SWindow> Window = GEngine && GEngine->GameViewport ? GEngine->GameViewport->GetWindow() : nullptr;
		if (Window.IsValid())
		{
			Gather(Window.ToSharedRef(), true, Out);
		}
		return Out;
	}

	/**
	 * True where a press in the middle of the button would reach it: nothing lies over it. Where something does, Over
	 * says what a press there would reach instead: the widget's kind, where in the code it was made, and its words, so
	 * that a replay names the layer and nobody has to guess it (v8425 part C, steps 25 and 31).
	 */
	bool Reachable(const FFoundButton& Button, FString* Over = nullptr, bool* bOffTheScreen = nullptr)
	{
		const TSharedPtr<SWindow> Window = GEngine && GEngine->GameViewport ? GEngine->GameViewport->GetWindow() : nullptr;
		if (!Window.IsValid() || !Button.Widget.IsValid())
		{
			return false;
		}
		const FGeometry& Place = Button.Widget->GetTickSpaceGeometry();
		const FVector2D Middle = Place.LocalToAbsolute(Place.GetLocalSize() * 0.5);
		const TArray<TSharedRef<SWindow>> Windows = { Window.ToSharedRef() };
		const FWidgetPath Under = FSlateApplication::Get().LocateWindowUnderMouse(Middle, Windows, false, 0);
		// What lies at the button's middle, from the window down to the topmost widget.
		TArray<TSharedRef<SWidget>> Path;
		if (Under.IsValid())
		{
			for (int32 Index = 0; Index < Under.Widgets.Num(); ++Index)
			{
				Path.Add(Under.Widgets[Index].Widget);
			}
		}
		if (Path.Num() == 0)
		{
			// (18 November 2026. A picture run's window is 1600 by 1000 whatever the Mac's screen is: Brian's is 1470 by
			// 956 points. The part of the window that hangs off the screen is no part of it to macOS, and a press there
			// "reached nothing" though the button was drawn and nothing lay over it: the plates at the right edge, the
			// sixth row of a list, the foot of a tall card all read as faults of the game. A player's own window is cut
			// to suit his screen, so this is the replay's trouble alone. There the window's own chart of what lies
			// where is asked instead, and the step's note says so.)
			for (const FWidgetAndPointer& Each : Window->GetHittestGrid().GetBubblePath(Middle, 0.0f, false, 0))
			{
				Path.Add(Each.Widget);
			}
			if (bOffTheScreen)
			{
				*bOffTheScreen = Path.Num() > 0;
			}
		}
		for (const TSharedRef<SWidget>& Each : Path)
		{
			if (&Each.Get() == Button.Widget.Get())
			{
				return true;
			}
		}
		if (Over)
		{
			if (Path.Num() == 0)
			{
				*Over = TEXT("nothing in the window takes a press there");
			}
			else
			{
				// The topmost widget at that point, and the two it sits in: enough to find the layer in the code.
				TArray<FString> Said;
				for (int32 Index = Path.Num() - 1; Index >= FMath::Max(0, Path.Num() - 3); --Index)
				{
					const TSharedRef<SWidget> Each = Path[Index];
					FString Words;
					Lettering(Each, Words);
					Said.Add(FString::Printf(TEXT("%s at %s%s"), *Each->GetTypeAsString(), *Each->GetReadableLocation(),
						Words.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" (\"%s\")"), *Words.Left(60))));
				}
				*Over = FString::Join(Said, TEXT(", in "));
			}
		}
		return false;
	}
}

bool FStepPlayer::Load(const FString& File, const FString& InFolder)
{
	TArray<FString> Lines;
	if (!FFileHelper::LoadFileToStringArray(Lines, *File))
	{
		UE_LOG(LogTheater, Error, TEXT("Steps: the step file could not be read: %s"), *File);
		return false;
	}
	Folder = InFolder;
	StepFolder = FPaths::GetPath(File);
	for (int32 Index = 0; Index < Lines.Num(); ++Index)
	{
		const FString Line = Lines[Index].TrimEnd();
		if (Line.TrimStart().IsEmpty() || Line.TrimStart().StartsWith(TEXT("#")))
		{
			continue;
		}
		TArray<FString> Parts;
		Line.ParseIntoArray(Parts, TEXT("\t"), false);
		FStep Step;
		Step.Line = Index + 1;
		Step.Verb = Parts.Num() > 0 ? Parts[0].TrimStartAndEnd().ToLower() : FString();
		Step.Target = Parts.Num() > 1 ? Parts[1].TrimStartAndEnd() : FString();
		Step.Expect = Parts.Num() > 2 ? Parts[2].TrimStartAndEnd() : FString();
		if (!Step.Verb.IsEmpty())
		{
			Steps.Add(Step);
		}
	}
	FFileHelper::SaveStringToFile(FString(), *FPaths::Combine(Folder, TEXT("steps.jsonl")));
	UE_LOG(LogTheater, Display, TEXT("Steps: %d steps to play from %s"), Steps.Num(), *File);
	return Steps.Num() > 0;
}

FStepPlayer::FSeen FStepPlayer::Look(ACampaignPlayerController& Player, int32 RegionId) const
{
	THEATER_SPEED_SCOPE(HarnessLook); // (the step player's own work, which a player does not pay for)
	FSeen Seen;
	Player.DescribeScreen(Seen.Open, Seen.Words, Seen.Hand);
	Seen.Notices = Player.StepNotices();
	if (FCampaignSession* Session = Player.GetSession())
	{
		Seen.Digest = Session->StepDigest(RegionId, Player.StepPicked(), Player.StepFleet(), Seen.Stamp, Seen.Holder);
		// (the game's state with no province named: what "did anything change" is judged by, whichever province a step names)
		Seen.Bare = Seen.Stamp;
		if (RegionId != INDEX_NONE)
		{
			FString NoHolder;
			Session->StepDigest(INDEX_NONE, Player.StepPicked(), Player.StepFleet(), Seen.Bare, NoHolder);
		}
	}
	return Seen;
}

bool FStepPlayer::Press(ACampaignPlayerController& Player, const FString& Words)
{
	// "WORDS" presses the first button that carries them; "WORDS#2" the second of several alike.
	FString Wanted = Words, Count;
	int32 Which = 1;
	if (Words.Split(TEXT("#"), &Wanted, &Count, ESearchCase::IgnoreCase, ESearchDir::FromEnd) && Count.IsNumeric())
	{
		Which = FMath::Max(1, FCString::Atoi(*Count));
	}
	else
	{
		Wanted = Words;
	}
	const FString Want = Plain(Wanted);
	const TArray<FFoundButton> All = ButtonsOnScreen();
	TArray<const FFoundButton*> Alike;
	for (const FFoundButton& Each : All)
	{
		if (Plain(Each.Words) == Want)
		{
			Alike.Add(&Each);
		}
	}
	if (Alike.Num() == 0)
	{
		for (const FFoundButton& Each : All)
		{
			if (Plain(Each.Words).Contains(Want))
			{
				Alike.Add(&Each);
			}
		}
	}
	if (!Alike.IsValidIndex(Which - 1))
	{
		bMissing = true;
		Note = FString::Printf(TEXT("no button on the screen carries the words \"%s\""), *Wanted);
		return false;
	}
	const FFoundButton& Button = *Alike[Which - 1];
	{
		// (the budget of "Smooth, easy play": nothing he must press lies beyond the window's edge)
		const TSharedPtr<SWindow> Window = GEngine && GEngine->GameViewport ? GEngine->GameViewport->GetWindow() : nullptr;
		const FGeometry& Place = Button.Widget->GetTickSpaceGeometry();
		const FVector2D From = Place.LocalToAbsolute(FVector2D::ZeroVector), To = Place.LocalToAbsolute(Place.GetLocalSize());
		const FSlateRect Frame = Window.IsValid() ? Window->GetRectInScreen() : FSlateRect();
		OnScreen = Window.IsValid() && From.X >= Frame.Left && From.Y >= Frame.Top && To.X <= Frame.Right && To.Y <= Frame.Bottom ? 1 : 0;
	}
	if (!Button.bEnabled)
	{
		// A button that cannot be pressed says why where the pointer rests on it, or it is mute.
		const TSharedPtr<IToolTip> Tip = Button.Widget->GetToolTip();
		if (Tip.IsValid() && !Tip->IsEmpty())
		{
			bSaidWhy = true;
			Note = TEXT("the button is dim, and says why where the pointer rests on it");
		}
		else
		{
			bMute = true;
			Note = TEXT("the button is dim and says nothing");
		}
		return false;
	}
	FString Over;
	bool bOffTheScreen = false;
	if (!Reachable(Button, &Over, &bOffTheScreen))
	{
		bError = true;
		Note = TEXT("the button is on the screen but something lies over it: a press would not reach it") + (Over.IsEmpty() ? FString() : TEXT(". At its middle: ") + Over);
		return false;
	}
	if (bOffTheScreen)
	{
		Note = TEXT("pressed where the test window hangs off the Mac's screen (the window's own chart was asked)");
	}
	StaticCastSharedPtr<SButton>(Button.Widget)->SimulateClick();
	return true;
}

void FStepPlayer::Sweep(ACampaignPlayerController& Player, const FStep& Step)
{
	// Every button on the open card, page or tray, one at a time: those that stand on the bare table are left
	// alone, and at most six of each kind are pressed.
	TMap<FString, int32> Seen;
	TArray<FStep> Presses;
	int32 Under = 0;
	for (const FFoundButton& Each : ButtonsOnScreen())
	{
		const FString Words = Plain(Each.Words);
		if (FrameButtons.Contains(Words))
		{
			continue;
		}
		// (A button of the table that the open card lies over is left alone too, and counted: the card is meant to
		// cover it. Each was pressed and written down as a fault, a dozen to a card, and the true faults were lost
		// among them. A button named by a step of its own is still tried, and still said to be covered if it is.)
		if (Each.bEnabled && !Reachable(Each))
		{
			++Under;
			continue;
		}
		int32& Count = Seen.FindOrAdd(Words);
		if (++Count > 6)
		{
			continue;
		}
		FStep One;
		One.Line = Step.Line;
		One.Verb = TEXT("press");
		One.Target = Count > 1 ? FString::Printf(TEXT("%s#%d"), *Each.Words, Count) : Each.Words;
		One.bSweep = true;
		Presses.Add(One);
	}
	if (FCampaignSession* Session = Player.GetSession())
	{
		SweepSave = Session->Save();
	}
	SweepContext = Context;
	bSweepDirty = false;
	Steps.Insert(Presses, At + 1);
	Note = FString::Printf(TEXT("%d buttons to press"), Presses.Num()) + (Under > 0 ? FString::Printf(TEXT(" (%d more lie under the open card and are left alone)"), Under) : FString());
}

void FStepPlayer::Act(ACampaignPlayerController& Player, const FStep& Step)
{
	FCampaignSession* Session = Player.GetSession();
	const ACampaignMapActor* Map = Player.GetMap();
	bMissing = bMute = bError = bSaidWhy = bMonth = bPan = bLook = false;
	bAir = bAirDid = bAirHandBack = false;
	AirWait.Reset();
	OnScreen = -1;
	Note.Reset();
	TargetRegion = INDEX_NONE;
	if (bStaged)
	{
		// (the pointer rested somewhere for the last step: it is lifted)
		Player.StageHover(INDEX_NONE);
		Player.StageGate(FString());
		bStaged = false;
	}
	int32 Region = INDEX_NONE, Sea = INDEX_NONE, Corps = INDEX_NONE, CorpsRegion = INDEX_NONE, Fleet = INDEX_NONE, Group = INDEX_NONE;
	auto Find = [&](const FString& Name)
	{
		// (the name was looked up before the press was timed: asking the rules again here would count against the game)
		if (bFound && Name == FoundName)
		{
			Region = FoundRegion;
			Sea = FoundSea;
			Corps = FoundCorps;
			CorpsRegion = FoundCorpsRegion;
			Fleet = FoundFleet;
			Group = FoundGroup;
			return true;
		}
		return Session && Session->StepFind(Name, Region, Sea, Corps, CorpsRegion, Fleet, Group);
	};
	const FString& Verb = Step.Verb;
	if (Verb == TEXT("start"))
	{
		// "USA 1939-09 seed 25 colonel"
		TArray<FString> Words;
		Step.Target.ParseIntoArray(Words, TEXT(" "), true);
		FString Year, Month;
		const bool bDated = Words.Num() > 1 && Words[1].Split(TEXT("-"), &Year, &Month);
		const int32 From = bDated ? (FCString::Atoi(*Year) - 1939) * 12 + FCString::Atoi(*Month) - 9 + 1 : 1;
		int32 Seed = 0;
		FString Level;
		for (int32 Index = 2; Index < Words.Num(); ++Index)
		{
			if (Words[Index] == TEXT("seed") && Words.IsValidIndex(Index + 1))
			{
				Seed = FCString::Atoi(*Words[++Index]);
			}
			else
			{
				Level = Words[Index];
			}
		}
		if (Words.Num() == 0 || !Player.BeginGrandFor(Words[0], From, Seed, Level))
		{
			bMissing = true;
			Note = TEXT("no such campaign could be begun");
		}
		Context.Reset();
	}
	else if (Verb == TEXT("snapshot"))
	{
		if (Session)
		{
			Saves.Add(Step.Target, Session->Save());
		}
		// (the buttons of the bare table are noted the first time the game is kept: a sweep leaves them alone)
		if (FrameButtons.Num() == 0)
		{
			for (const FFoundButton& Each : ButtonsOnScreen())
			{
				FrameButtons.AddUnique(Plain(Each.Words));
			}
		}
	}
	else if (Verb == TEXT("restore"))
	{
		const FString* Kept = Saves.Find(Step.Target);
		if (!Kept || !Player.RestoreCampaign(*Kept))
		{
			bMissing = true;
			Note = TEXT("nothing was kept under that name");
		}
		Context.Reset();
	}
	else if (Verb == TEXT("load"))
	{
		// A saved game, by its file: a full path, or a name beside the step file. (The save is the rules' own text, the
		// same in the browser game and here.)
		FString Text;
		const FString Path = FPaths::IsRelative(Step.Target) ? FPaths::Combine(StepFolder, Step.Target) : Step.Target;
		if (!FFileHelper::LoadFileToString(Text, *Path) || !Player.RestoreCampaign(Text))
		{
			bMissing = true;
			Note = TEXT("no saved game could be opened from that file");
		}
		Context.Reset();
	}
	else if (Verb == TEXT("select-province") || Verb == TEXT("check-province"))
	{
		if (!Find(Step.Target) || Region == INDEX_NONE)
		{
			bMissing = true;
			Note = TEXT("no province of that name");
		}
		else
		{
			TargetRegion = Region;
			if (Verb == TEXT("select-province") && !Player.IsTableBusy())
			{
				// A press on the flag of his own army there picks it up (or, where it takes no orders, shows it);
				// anywhere else it is a click on the ground.
				const FString Me = Player.GetPlayerTag();
				const bool bMine = Session && Session->GetView().Corps.ContainsByPredicate([Region, &Me](const FViewCorps& Each) { return Each.Region == Region && Each.Nation == Me; });
				if (!Player.PickUpArmyIn(Region))
				{
					if (bMine)
					{
						Player.LookAtArmy(Region, Me);
						Player.SelectRegion(Region);
					}
					else
					{
						Player.ClickRegion(Region);
					}
				}
			}
			else if (Verb == TEXT("select-province"))
			{
				Note = TEXT("a card or a page lies over the table: the click does not reach it");
			}
		}
	}
	else if (Verb == TEXT("select-unit"))
	{
		if (!Find(Step.Target) || (Corps == INDEX_NONE && Fleet == INDEX_NONE && Group == INDEX_NONE))
		{
			bMissing = true;
			Note = TEXT("no corps, fleet or air group of that name");
		}
		else if (Corps != INDEX_NONE)
		{
			TargetRegion = CorpsRegion;
			if (!Player.PickUpArmyIn(CorpsRegion))
			{
				Player.LookAtArmy(CorpsRegion, Player.GetPlayerTag());
				Player.SelectRegion(CorpsRegion);
			}
		}
		else if (Fleet != INDEX_NONE)
		{
			Player.PickUpFleet(Fleet);
		}
		else
		{
			Player.OpenPage(FString::Printf(TEXT("group:%d"), Group));
		}
	}
	else if (Verb == TEXT("order-province"))
	{
		if (!Find(Step.Target) || Region == INDEX_NONE)
		{
			bMissing = true;
			Note = TEXT("no province of that name");
		}
		else
		{
			TargetRegion = Region;
			if (!Player.IsTableBusy())
			{
				Player.RightClickOn(Region, INDEX_NONE, true);
			}
			else
			{
				Note = TEXT("a card or a page lies over the table: the click does not reach it");
			}
		}
	}
	else if (Verb == TEXT("order-unit"))
	{
		// A right-click on the figure of an army seen on the table (an enemy's, for an attack), with what is in hand.
		int32 Aimed = INDEX_NONE, Ground = INDEX_NONE;
		if (!Find(Step.Target) || Corps == INDEX_NONE)
		{
			bMissing = true;
			Note = TEXT("no corps of that name");
		}
		else if (!Player.RightClickOnFigure(Corps, Aimed, Ground))
		{
			bMissing = true;
			Note = TEXT("that corps has no figure on the table");
		}
		else
		{
			TargetRegion = CorpsRegion;
			auto Named = [Map](int32 Id) { return Map && Map->GetData().Regions.IsValidIndex(Id) ? Map->GetData().Regions[Id].Name : FString(TEXT("no province")); };
			Note = FString::Printf(TEXT("the figure stands in %s; the ground behind the pointer is %s; the order went to %s%s"), *Named(CorpsRegion), *Named(Ground), *Named(Aimed),
				Player.IsTableBusy() && Aimed == INDEX_NONE ? TEXT(" (a card lay over the table)") : TEXT(""));
		}
	}
	else if (Verb == TEXT("ask-war"))
	{
		// The WAR button on that country's row in the Foreign Office (a row may lie below the fold of its list): the
		// question's card opens, as the button opens it. DECLARE WAR on the card is pressed by its words.
		const FCampaignNation* Asked = nullptr;
		if (Map)
		{
			for (const auto& Pair : Map->GetData().Nations)
			{
				const FCampaignNation& Each = Pair.Value;
				if (Each.Name.Equals(Step.Target, ESearchCase::IgnoreCase) || Each.Tag.Equals(Step.Target, ESearchCase::IgnoreCase))
				{
					Asked = &Each;
				}
			}
		}
		if (!Asked)
		{
			bMissing = true;
			Note = TEXT("no country of that name");
		}
		else
		{
			Player.AskWarOn(Asked->Tag);
		}
	}
	else if (Verb == TEXT("order-sea") || Verb == TEXT("select-sea"))
	{
		if (!Find(Step.Target) || Sea == INDEX_NONE)
		{
			bMissing = true;
			Note = TEXT("no sea of that name");
		}
		else if (Verb == TEXT("select-sea"))
		{
			// (on this table a fleet is sent with a right-click, as an army is: a left click on open water picks nothing up)
			Note = TEXT("a left click on a sea gives no order on the 3D table: a fleet is sent with a right-click");
		}
		else if (!Player.IsTableBusy())
		{
			Player.RightClickOn(INDEX_NONE, Sea, true);
		}
	}
	else if (Verb == TEXT("point-at") || Verb == TEXT("click-mark"))
	{
		// "airfield Washington", "gate Panama Canal", or a province's name
		FString Kind, Name;
		if (!Step.Target.Split(TEXT(" "), &Kind, &Name) || (Kind != TEXT("airfield") && Kind != TEXT("gate")))
		{
			Kind.Reset();
			Name = Step.Target;
		}
		const bool bClick = Verb == TEXT("click-mark");
		if (Kind == TEXT("gate"))
		{
			if (!bClick)
			{
				Player.StageGate(Name);
				bStaged = true;
			}
			else if (Player.IsTableBusy())
			{
				Note = TEXT("a card or a page lies over the table: the click does not reach it");
			}
			else if (!Player.ClickGate(Name))
			{
				bMissing = true;
				Note = TEXT("no gate of that name");
			}
		}
		else if (!Find(Name) || Region == INDEX_NONE)
		{
			bMissing = true;
			Note = TEXT("no such place");
		}
		else
		{
			TargetRegion = Region;
			if (!bClick)
			{
				Player.StageHover(Region, Kind == TEXT("airfield"));
				bStaged = true;
			}
			else if (Player.IsTableBusy())
			{
				Note = TEXT("a card or a page lies over the table: the click does not reach it");
			}
			else if (Kind == TEXT("airfield"))
			{
				Player.ClickAirfield(Region);
			}
			else
			{
				Player.ClickRegion(Region);
			}
		}
	}
	else if (Verb == TEXT("pan-zoom") || Verb == TEXT("look-at"))
	{
		// "Paris near", "Norfolk middle", "Ruhr far": ten seconds of pan and zoom there, for the frame rate
		// ("look-at" goes there and stays: its picture is of that place, taken once its fine ground stands)
		FString Name = Step.Target, Height;
		int32 Cut = INDEX_NONE;
		if (Step.Target.FindLastChar(TEXT(' '), Cut))
		{
			Height = Step.Target.Mid(Cut + 1).ToLower();
			if (Height == TEXT("near") || Height == TEXT("middle") || Height == TEXT("far"))
			{
				Name = Step.Target.Left(Cut);
			}
		}
		ACampaignCameraPawn* Eye = Cast<ACampaignCameraPawn>(Player.GetPawn());
		if (!Find(Name) || Region == INDEX_NONE || !Map || !Eye || !Map->GetData().Regions.IsValidIndex(Region))
		{
			bMissing = true;
			Note = TEXT("no such place");
		}
		else
		{
			TargetRegion = Region;
			const FVector2D Spot = Map->GetData().Regions[Region].Center;
			Eye->GoTo(Map->MapToWorld(Spot.X, Spot.Y), Height == TEXT("near") ? 0.2f : Height == TEXT("far") ? 0.8f : 0.45f, true);
			bPan = Verb == TEXT("pan-zoom");
			bLook = !bPan;
		}
	}
	else if (Verb == TEXT("open-page"))
	{
		const FString Page = Step.Target.ToLower();
		if (Page == TEXT("build")) { Player.OpenArsenal(); }
		else if (Page == TEXT("research")) { Player.OpenResearch(); }
		else if (Page == TEXT("air")) { Player.OpenPage(TEXT("raids")); }
		else if (Page == TEXT("navy")) { Player.OpenForces(); }
		else if (Page == TEXT("powers")) { Player.OpenDiplomacy(); }
		else if (Page == TEXT("staff")) { Player.OpenStaff(); }
		else if (Page == TEXT("ledger")) { Player.OpenLedger(); }
		else if (Page == TEXT("news") || Page == TEXT("battles")) { Player.OpenReports(); }
		else if (Page == TEXT("generals")) { Player.OpenGenerals(); }
		else if (Page == TEXT("honours")) { Player.OpenRoll(); }
		else if (Page == TEXT("region")) { Note = TEXT("the province's card is the table's own panel here: it opens with a click on the province"); }
		else { Player.OpenPage(Page); }
	}
	else if (Verb == TEXT("press"))
	{
		Press(Player, Step.Target);
	}
	else if (Verb == TEXT("press-each"))
	{
		Sweep(Player, Step);
	}
	else if (Verb == TEXT("close"))
	{
		Player.PressEscape();
	}
	else if (Verb == TEXT("key"))
	{
		// A key of the keyboard, by the hand that key is bound to.
		const FString Key = Step.Target.ToLower();
		if (Key == TEXT("tab")) { Player.PressTab(); }
		else if (Key == TEXT("enter") || Key == TEXT("return")) { bMonth = !Player.IsTableBusy() && !Player.GetEndAsk().bVisible; Player.PressEnter(); }
		else if (Key == TEXT("esc") || Key == TEXT("escape")) { Player.PressEscape(); }
		else if (Key == TEXT("space")) { Player.PressSpace(); }
		else if (Key == TEXT("t")) { Player.ToggleDayNight(); }
		else if (Key == TEXT("r")) { Player.OpenReports(); }
		else
		{
			bMissing = true;
			Note = TEXT("the hook does not press this key yet");
		}
	}
	else if (Verb == TEXT("end-month"))
	{
		bMonth = true;
		Player.EndMonth();
		Context.Reset();
	}
	else if (Verb == TEXT("air-key") || Verb == TEXT("air-wait"))
	{
		ActAir(Player, Step);
	}
	else
	{
		bMissing = true;
		Note = TEXT("the 3D table does not know this verb");
	}
	// (a press that opened his own air battle, FLY IT YOURSELF, is settled as an air step: the raid keeps the table busy
	// until it is handed back, so the picture is taken a second in, in the opening flyover, and the next line follows)
	if (Verb == TEXT("press") && !bAir && Player.IsFightingLive())
	{
		AirOpen = OpenAirBattle(Player);
		bAir = AirOpen.IsValid();
		AirSettle = 1.1f;
	}
	// The steps that open a card or put something in hand are remembered: a sweep plays them again after it restores.
	if (!Step.bReplay && !Step.bSweep)
	{
		if (Verb == TEXT("select-province") || Verb == TEXT("select-unit") || Verb == TEXT("open-page"))
		{
			FStep Again = Step;
			Again.bReplay = true;
			Context.Add(Again);
			if (Context.Num() > 3)
			{
				Context.RemoveAt(0);
			}
		}
		else if (Verb != TEXT("press-each") && Verb != TEXT("point-at") && Verb != TEXT("check-province") && Verb != TEXT("snapshot"))
		{
			Context.Reset();
		}
	}
}

void FStepPlayer::ActAir(ACampaignPlayerController& Player, const FStep& Step)
{
	// The screen's keys are played on the battle as SAirBattleScreen::OnKeyDown plays them.
	bAir = true;
	AirSettle = 0.3f;
	AirOpen = OpenAirBattle(Player);
	AAirBattle* Battle = AirOpen.Get();
	const FAirBattleState* S = Battle ? Battle->GetState() : nullptr;
	const FString What = Step.Target.TrimStartAndEnd().ToLower();
	if (!S)
	{
		Note = TEXT("no air battle is up");
		return;
	}
	if (Step.Verb == TEXT("air-wait"))
	{
		const bool bReal = What.EndsWith(TEXT("s")) && What.LeftChop(1).IsNumeric();
		if (What == TEXT("flyover") || What == TEXT("card") || (What.IsNumeric() && FCString::Atod(*What) >= 0.0) || bReal)
		{
			AirWait = What;
		}
		else
		{
			bMissing = true;
			Note = TEXT("air-wait takes battle seconds (212), real seconds (20s), flyover or card");
		}
		return;
	}
	if (S->bFlying)
	{
		// (as on the keyboard: any key during the flyover takes command, and does nothing else)
		Battle->Press(TEXT("skip"));
		bAirDid = true;
		AirSettle = 1.5f;
		Note = What == TEXT("skip") ? FString(TEXT("the flyover skipped")) : FString(TEXT("pressed during the flyover: it only skips it, as any key does"));
		return;
	}
	if (What == TEXT("skip"))
	{
		Note = TEXT("no flyover is running");
		return;
	}
	const bool bClose = S->bClose;
	const int32 Plane = S->Subject.Plane, Group = S->RideGroup;
	const double Speed = S->Clock.Speed;
	if (What == TEXT("continue") || What == TEXT("return") || What == TEXT("enter"))
	{
		if (S->bCard)
		{
			Battle->Press(TEXT("continue"));
			bAirDid = true;
			bAirHandBack = true; // (the battle is handed back to the map: the step waits, as any, for the table to be still)
			AirSettle = 1.1f;
		}
		else if (S->Question != 0)
		{
			Battle->Answer(false);
			bAirDid = true;
			Note = TEXT("KEEP FIGHTING (the question's default)");
		}
		else
		{
			Note = TEXT("no after card is up");
		}
	}
	else if (What == TEXT("escape") || What == TEXT("esc"))
	{
		if (S->Question != 0) { Battle->Answer(false); bAirDid = true; }
		else if (S->bClose && !S->bCard) { Battle->Press(TEXT("ride")); bAirDid = true; AirSettle = 1.5f; }
		else { Note = TEXT("nothing for Esc to close"); }
	}
	else if (What == TEXT("chase") || What == TEXT("c"))
	{
		Battle->Press(TEXT("ride"));
		bAirDid = S->bClose != bClose;
		AirSettle = 1.5f; // (the eye glides to the squadron's leader)
		Note = S->bClose ? TEXT("the chase view") : TEXT("the strategy view");
	}
	else if (What == TEXT("next") || What == TEXT("prev") || What == TEXT("up") || What == TEXT("down")
		|| What == TEXT("right") || What == TEXT("left"))
	{
		if (!S->bClose || S->bCard)
		{
			Note = TEXT("the arrows step only in the chase view");
		}
		else
		{
			const FString Way = What == TEXT("right") ? FString(TEXT("next")) : What == TEXT("left") ? FString(TEXT("prev")) : What;
			Battle->Press(TEXT("ride:") + Way);
			bAirDid = S->Subject.Plane != Plane || S->RideGroup != Group;
			AirSettle = 1.5f;
			if (!bAirDid)
			{
				Note = TEXT("no other aircraft or squadron that way");
			}
		}
	}
	else if (What == TEXT("pause") || What == TEXT("play") || What == TEXT("space"))
	{
		const bool bHold = What == TEXT("pause") || (What == TEXT("space") && Speed > 0.0);
		if (bHold && Speed > 0.0)
		{
			AirSpeedBefore = Speed;
			Battle->SetSpeed(0.0);
		}
		else if (!bHold && Speed <= 0.0)
		{
			Battle->SetSpeed(AirSpeedBefore > 0.0 ? AirSpeedBefore : 1.0);
		}
		bAirDid = S->Clock.Speed != Speed;
		if (!bAirDid)
		{
			Note = bHold ? TEXT("already held") : TEXT("already running");
		}
	}
	else if (What.StartsWith(TEXT("x")) && What.Mid(1).IsNumeric())
	{
		const double Want = FCString::Atod(*What.Mid(1));
		if (Want == 0.5 || Want == 1.0 || Want == 2.0 || Want == 4.0)
		{
			AirSpeedBefore = Want;
			Battle->SetSpeed(Want);
			bAirDid = S->Clock.Speed != Speed;
			if (!bAirDid)
			{
				Note = TEXT("already at that speed");
			}
		}
		else
		{
			bMissing = true;
			Note = TEXT("the battle runs at x0.5, x1, x2 or x4");
		}
	}
	else
	{
		bMissing = true;
		Note = TEXT("the hook does not press this key of the air battle yet");
	}
}

bool FStepPlayer::AirWaiting() const
{
	const AAirBattle* Battle = AirOpen.Get();
	const FAirBattleState* S = Battle ? Battle->GetState() : nullptr;
	if (!S || AirWait.IsEmpty())
	{
		return false; // (handed back, or nothing asked: nothing left to wait for)
	}
	if (AirWait == TEXT("card"))
	{
		return !S->bCard;
	}
	if (AirWait == TEXT("flyover"))
	{
		return S->bFlying || !S->bFlyTried;
	}
	if (AirWait.EndsWith(TEXT("s")))
	{
		return !S->bCard && Clock < FCString::Atof(*AirWait.LeftChop(1)); // (real seconds from this step)
	}
	return !S->bCard && S->BattleT < FCString::Atod(*AirWait);
}

FString FStepPlayer::AirNow() const
{
	const AAirBattle* Battle = AirOpen.Get();
	const FAirBattleState* S = Battle ? Battle->GetState() : nullptr;
	if (!S)
	{
		return TEXT("the air battle handed back");
	}
	return FString::Printf(TEXT("battle %.0f s, x%g%s%s%s%s"), S->BattleT, S->Clock.Speed, S->bFlying ? TEXT(", the flyover") : TEXT(""),
		S->bClose ? TEXT(", the chase view") : TEXT(""), S->bOver ? TEXT(", over") : TEXT(""), S->bCard ? TEXT(", the after card up") : TEXT(""));
}

void FStepPlayer::Log(ACampaignPlayerController& Player, const FStep& Step)
{
	const FSeen After = Look(Player, TargetRegion);
	// (an air step is judged on the battle it was given to, which the table's reading does not see)
	const bool bChanged = bAirDid || After.Bare != Before.Bare || After.Open != Before.Open || After.Hand != Before.Hand;
	// (the pointer's slip counts as an answer only for a step that points: for a click it only names what is under the pointer)
	const bool bPointing = Step.Verb == TEXT("point-at");
	FString Words = After.Words;
	if (bPointing)
	{
		Words = Player.GetHoverTip().ToString();
	}
	// (a sentence put on the paper line during the step is an answer, though it be the sentence that already stood there)
	const bool bSpoke = After.Notices != Before.Notices;
	const bool bSaid = bSaidWhy || bSpoke || (bPointing ? !Words.IsEmpty() : (!After.Words.IsEmpty() && After.Words != Before.Words));
	const FString Verdict = bError ? TEXT("ERROR") : bMissing ? TEXT("MISSING") : bMute ? TEXT("MUTE") : bChanged ? TEXT("DID") : bSaid ? TEXT("SAID WHY") : TEXT("DEAD");
	// What the step expected of the game.
	FString Pass = TEXT("null");
	const FString Expect = Step.Expect;
	if (Expect == TEXT("does")) { Pass = Verdict == TEXT("DID") ? TEXT("true") : TEXT("false"); }
	// ("says-why": nothing in the game changes and words say why. On this table a click on a place also looks at it, so
	// a card of the place may open beside the words: the game itself must be as it was.)
	else if (Expect == TEXT("says-why")) { Pass = (Verdict == TEXT("SAID WHY") || (bSaid && !bError && !bMissing && After.Bare == Before.Bare)) ? TEXT("true") : TEXT("false"); }
	else if (Expect == TEXT("moves")) { Pass = After.Hand != Before.Hand && After.Bare != Before.Bare ? TEXT("true") : TEXT("false"); }
	else if (Expect.StartsWith(TEXT("holder"))) { Pass = After.Holder == Expect.Mid(6).TrimStartAndEnd() ? TEXT("true") : TEXT("false"); }
	else if (Expect.StartsWith(TEXT("says ")))
	{
		// "says WORDS": the words are somewhere in what he is shown (the line under the map, the card or page on top, the slip of a step that points)
		const FString Shown = Plain(Words + TEXT(" ") + After.Words + TEXT(" ") + After.Open);
		TArray<FString> Each;
		Plain(Expect.Mid(5)).ParseIntoArray(Each, TEXT(" "), true);
		bool bAll = Each.Num() > 0;
		for (const FString& Word : Each) { bAll = bAll && Shown.Contains(Word); }
		Pass = bAll ? TEXT("true") : TEXT("false");
	}
	else if (Expect.StartsWith(TEXT("card")))
	{
		const FString Title = Plain(Expect.Mid(4));
		Pass = !After.Open.IsEmpty() && (Title.IsEmpty() || Plain(After.Open).Contains(Title)) ? TEXT("true") : TEXT("false");
	}
	++Logged;
	const FString Picture = FString::Printf(TEXT("step-%03d.png"), Logged);
	// How smooth it was: the frames' lengths in order of size give the lowest and the middle frame rate.
	TArray<float> Sorted = Frames;
	Sorted.Sort();
	const float Longest = Sorted.Num() > 0 ? Sorted.Last() : 0.0f, Middle = Sorted.Num() > 0 ? Sorted[Sorted.Num() / 2] : 0.0f;
	const int32 FrameCount = FMath::Max(Frames.Num(), 1);
	FString Came;
	for (const FString& Each : Screens)
	{
		Came += (Came.IsEmpty() ? TEXT("") : TEXT(",")) + Quote(Each);
	}
	const FString Smooth = FString::Printf(TEXT("\"act_ms\":%.0f,\"first_ms\":%s,\"settled_ms\":%s,\"wait_ms\":%.0f,\"fps_low\":%.1f,\"fps_mid\":%.1f,\"game_ms\":%.1f,\"draw_ms\":%.1f,\"chip_ms\":%.1f,\"on_screen\":%s,\"screens\":[%s]"),
		ActMs, FirstMs < 0.0 ? TEXT("null") : *FString::Printf(TEXT("%.0f"), FirstMs), LastMs < 0.0 ? TEXT("null") : *FString::Printf(TEXT("%.0f"), LastMs), SinceAct * 1000.0,
		Longest > 0.0f ? 1.0f / Longest : 0.0f, Middle > 0.0f ? 1.0f / Middle : 0.0f, GameMs / FrameCount, DrawMs / FrameCount, ChipMs / FrameCount, OnScreen < 0 ? TEXT("null") : OnScreen > 0 ? TEXT("true") : TEXT("false"), *Came);
	const FString Line = FString::Printf(TEXT("{\"n\":%d,\"line\":%d,\"verb\":%s,\"target\":%s,\"expect\":%s,\"hand\":%s,\"open\":%s,\"words\":%s,\"before\":%s,\"after\":%s,\"verdict\":%s,\"said\":%s,\"pass\":%s,\"note\":%s,%s,\"picture\":%s}\n"),
		Logged, Step.Line, *Quote(Step.bSweep ? FString(TEXT("press-each: press")) : Step.Verb), *Quote(Step.Target), *Quote(Step.Expect), *Quote(After.Hand), *Quote(After.Open), *Quote(Words),
		Before.Digest.IsEmpty() ? TEXT("null") : *Before.Digest, After.Digest.IsEmpty() ? TEXT("null") : *After.Digest, *Quote(Verdict), bSaid ? TEXT("true") : TEXT("false"), *Pass, *Quote(Note), *Smooth, *Quote(Picture));
	FFileHelper::SaveStringToFile(Line, *FPaths::Combine(Folder, TEXT("steps.jsonl")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
	FScreenshotRequest::RequestScreenshot(FPaths::Combine(Folder, Picture), true, false);
	UE_LOG(LogTheater, Display, TEXT("Steps: %d (line %d) %s %s: %s%s"), Logged, Step.Line, *Step.Verb, *Step.Target, *Verdict, Pass == TEXT("false") ? TEXT("  ** NOT AS EXPECTED **") : TEXT(""));
	// A press of a sweep that did anything: the game is put back as it stood, and the card or the army with it.
	if (Step.bSweep && bChanged)
	{
		bSweepDirty = true;
	}
}

bool FStepPlayer::Tick(ACampaignPlayerController& Player, float DeltaSeconds)
{
	if (!Steps.IsValidIndex(At))
	{
		return false;
	}
	Clock += DeltaSeconds;
	const FStep Step = Steps[At]; // (a copy: a sweep adds its presses to the list)
	if (Phase == 0)
	{
		// Wait for the table to be still: a month's replay, a newsreel. (An air step is played in the raid, which keeps
		// the table busy until it is handed back: it does not wait.)
		const bool bAirStep = Step.Verb.StartsWith(TEXT("air-"));
		if (Clock < 0.25f || (!bAirStep && Player.IsBusyForSteps() && Clock < 45.0f))
		{
			return true;
		}
		if (Step.bSweep && bSweepDirty && !SweepSave.IsEmpty())
		{
			// (the last press changed the game: back to where the sweep began, and its card open again)
			bSweepDirty = false;
			Player.RestoreCampaign(SweepSave);
			Steps.Insert(SweepContext, At);
			Clock = 0.0f;
			return true;
		}
		// The game as it stands before the step, with the holder of the province the step names where its name can be
		// found beforehand. (An order is carried out the moment it is given: a reading taken after it is not "before".)
		int32 Named = INDEX_NONE;
		if (FCampaignSession* Session = Player.GetSession())
		{
			FString Name = Step.Target;
			for (const TCHAR* Lead : { TEXT("airfield "), TEXT("gate ") })
			{
				if (Name.StartsWith(Lead)) { Name = Name.Mid(FCString::Strlen(Lead)); }
			}
			for (const TCHAR* Tail : { TEXT(" near"), TEXT(" middle"), TEXT(" far") })
			{
				if (Name.EndsWith(Tail)) { Name = Name.LeftChop(FCString::Strlen(Tail)); }
			}
			int32 Region = INDEX_NONE, Sea = INDEX_NONE, Corps = INDEX_NONE, CorpsRegion = INDEX_NONE, Fleet = INDEX_NONE, Group = INDEX_NONE;
			const bool bOfUnit = Step.Verb == TEXT("select-unit") || Step.Verb == TEXT("order-unit");
			bFound = false;
			if (!Name.IsEmpty() && !Step.bReplay && !bAirStep && Step.Verb != TEXT("press") && Step.Verb != TEXT("key") && Step.Verb != TEXT("load") && Step.Verb != TEXT("start") && Step.Verb != TEXT("ask-war")
				&& Session->StepFind(Name, Region, Sea, Corps, CorpsRegion, Fleet, Group))
			{
				Named = bOfUnit ? CorpsRegion : Region;
				bFound = true;
				FoundName = Name;
				FoundRegion = Region;
				FoundSea = Sea;
				FoundCorps = Corps;
				FoundCorpsRegion = CorpsRegion;
				FoundFleet = Fleet;
				FoundGroup = Group;
			}
		}
		Before = Look(Player, Named);
		ShownBefore = ShownLast = Before.Open + TEXT("|") + Before.Words + TEXT("|") + Before.Hand;
		// (That reading asked the rules several questions, and its frame is a long one. It is let go by before the
		// press, so that what is timed after the press is the game's own work and not the hook's.)
		Phase = 3;
		Clock = 0.0f;
		LookFrames = 0;
		return true;
	}
	if (Phase == 3)
	{
		if (++LookFrames < 3 || Clock < 0.12f)
		{
			return true;
		}
		const double Began = FPlatformTime::Seconds();
		Act(Player, Step);
		ActMs = (FPlatformTime::Seconds() - Began) * 1000.0;
		// (Time is read off the clock from here: the engine caps the length it reports for a frame at 0.4 s. The first
		// frame counted runs from the press itself, so the press's own work is in it.)
		ActBegan = Began;
		LastTickAt = Began;
		FirstMs = LastMs = -1.0;
		SinceAct = 0.0;
		Frames.Reset();
		Screens.Reset();
		GameMs = DrawMs = ChipMs = 0.0;
		Phase = 1;
		Clock = 0.0f;
		return true;
	}
	if (Phase == 1)
	{
		// How smooth it is while it happens: every frame's length, and when what he is shown first changed and last changed.
		{
			const double NowAt = FPlatformTime::Seconds();
			Frames.Add(static_cast<float>(NowAt - LastTickAt));
			LastTickAt = NowAt;
			SinceAct = NowAt - ActBegan;
		}
		GameMs += FPlatformTime::ToMilliseconds(GGameThreadTime);
		DrawMs += FPlatformTime::ToMilliseconds(GRenderThreadTime);
		ChipMs += FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles());
		{
			FString Open, Words, Hand;
			Player.DescribeScreen(Open, Words, Hand);
			const FString Shown = Open + TEXT("|") + Words + TEXT("|") + Hand;
			if (Shown != ShownLast)
			{
				LastMs = SinceAct * 1000.0; // (from the press, by the clock)
				if (FirstMs < 0.0 && Shown != ShownBefore)
				{
					FirstMs = LastMs;
				}
				ShownLast = Shown;
			}
			if (bMonth)
			{
				const FString State = Player.StepState();
				if (!State.IsEmpty() && (Screens.Num() == 0 || !Screens.Last().StartsWith(State + TEXT(" @"))))
				{
					Screens.Add(FString::Printf(TEXT("%s @ %.1f s"), *State, SinceAct));
				}
			}
		}
		if (bPan)
		{
			// Ten seconds of pan and zoom: across the table and back, in and out.
			if (ACampaignCameraPawn* Eye = Cast<ACampaignCameraPawn>(Player.GetPawn()))
			{
				Eye->Pan(FVector2D(FMath::Sin(Clock * 0.63f), 0.5f * FMath::Cos(Clock * 0.41f)), DeltaSeconds);
				if (FMath::FloorToInt32(Clock / 1.25f) != FMath::FloorToInt32((Clock - DeltaSeconds) / 1.25f))
				{
					Eye->ZoomBy(FMath::FloorToInt32(Clock / 1.25f) % 2 == 0 ? 1.0f : -1.0f);
				}
			}
			if (Clock < 10.0f)
			{
				return true;
			}
		}
		if (bLook && Clock < 15.0f)
		{
			// (the place's fine ground is waited for, and a moment more for its towns and trees)
			const ACampaignMapActor* Table = Player.GetMap();
			if (Clock < 2.0f || (Table && Table->AreFineSheetsComing()))
			{
				LookSettled = Clock;
				return true;
			}
			if (Clock < LookSettled + 1.5f)
			{
				return true;
			}
		}
		// Let it happen: a march is flown across the table, a month is played through, a card opens. (An air step: the
		// battle's own clock is waited on, and the table only once the battle is handed back.)
		const float Settle = bMonth ? 2.5f : bAir ? AirSettle : 1.1f;
		const bool bBusy = Player.IsBusyForSteps() && (!bAir || bAirHandBack);
		if (Clock < Settle || (bBusy && Clock < (bMonth ? 120.0f : 20.0f)) || (!AirWait.IsEmpty() && Clock < 900.0f && AirWaiting()))
		{
			if (bMonth && Clock > 1.0f)
			{
				Player.StepThroughMonth(); // (the questions and the screens a month's end brings are answered as a player in a hurry would)
			}
			return true;
		}
		if (bAir)
		{
			if (!AirWait.IsEmpty())
			{
				const bool bCame = !AirWaiting() && AirOpen.IsValid();
				bAirDid = bCame;
				Note = bCame ? FString() : AirOpen.IsValid() ? FString(TEXT("waited 15 minutes: ")) : FString(TEXT("the battle ended before it came: "));
			}
			Note = Note.IsEmpty() ? AirNow() : Note + (Note.EndsWith(TEXT(": ")) ? TEXT("") : TEXT("; ")) + AirNow();
		}
		if (!Step.bReplay)
		{
			Log(Player, Step);
		}
		Phase = 2;
		Clock = 0.0f;
		return true;
	}
	// A moment for the picture to be written, and on.
	if (Clock < (Step.bReplay ? 0.05f : 0.45f))
	{
		return true;
	}
	++At;
	Phase = 0;
	Clock = 0.0f;
	if (!Steps.IsValidIndex(At))
	{
		UE_LOG(LogTheater, Display, TEXT("Steps: all %d played and logged"), Logged);
		return false;
	}
	return true;
}
