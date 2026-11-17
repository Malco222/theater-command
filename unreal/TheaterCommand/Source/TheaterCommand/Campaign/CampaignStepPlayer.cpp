#include "Campaign/CampaignStepPlayer.h"
#include "Campaign/CampaignCameraPawn.h"
#include "Campaign/CampaignMapActor.h"
#include "Campaign/CampaignPlayerController.h"
#include "Campaign/CampaignSession.h"
#include "Campaign/TheaterSpeed.h"
#include "TheaterCommand.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
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

	/** True where a press in the middle of the button would reach it: nothing lies over it. */
	bool Reachable(const FFoundButton& Button)
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
		return Under.IsValid() && Under.ContainsWidget(Button.Widget.Get());
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
	if (!Reachable(Button))
	{
		bError = true;
		Note = TEXT("the button is on the screen but something lies over it: a press would not reach it");
		return false;
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
	for (const FFoundButton& Each : ButtonsOnScreen())
	{
		const FString Words = Plain(Each.Words);
		if (FrameButtons.Contains(Words))
		{
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
	Note = FString::Printf(TEXT("%d buttons to press"), Presses.Num());
}

void FStepPlayer::Act(ACampaignPlayerController& Player, const FStep& Step)
{
	FCampaignSession* Session = Player.GetSession();
	const ACampaignMapActor* Map = Player.GetMap();
	bMissing = bMute = bError = bSaidWhy = bMonth = bPan = false;
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
	else if (Verb == TEXT("pan-zoom"))
	{
		// "Paris near", "Norfolk middle", "Ruhr far": ten seconds of pan and zoom there, for the frame rate
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
			bPan = true;
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
		if (Key == TEXT("tab")) { Player.NextArmy(); }
		else if (Key == TEXT("enter") || Key == TEXT("return")) { bMonth = !Player.IsTableBusy(); Player.EndMonth(); }
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
	else
	{
		bMissing = true;
		Note = TEXT("the 3D table does not know this verb");
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

void FStepPlayer::Log(ACampaignPlayerController& Player, const FStep& Step)
{
	const FSeen After = Look(Player, TargetRegion);
	const bool bChanged = After.Bare != Before.Bare || After.Open != Before.Open || After.Hand != Before.Hand;
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
		// Wait for the table to be still: a month's replay, a newsreel.
		if (Clock < 0.25f || (Player.IsBusyForSteps() && Clock < 45.0f))
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
			if (!Name.IsEmpty() && !Step.bReplay && Step.Verb != TEXT("press") && Step.Verb != TEXT("key") && Step.Verb != TEXT("load") && Step.Verb != TEXT("start")
				&& Session->StepFind(Name, Region, Sea, Corps, CorpsRegion, Fleet, Group))
			{
				Named = bOfUnit ? CorpsRegion : Region;
			}
		}
		Before = Look(Player, Named);
		ShownBefore = ShownLast = Before.Open + TEXT("|") + Before.Words + TEXT("|") + Before.Hand;
		const double Began = FPlatformTime::Seconds();
		Act(Player, Step);
		ActMs = (FPlatformTime::Seconds() - Began) * 1000.0;
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
		SinceAct += DeltaSeconds;
		Frames.Add(DeltaSeconds);
		GameMs += FPlatformTime::ToMilliseconds(GGameThreadTime);
		DrawMs += FPlatformTime::ToMilliseconds(GRenderThreadTime);
		ChipMs += FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles());
		{
			FString Open, Words, Hand;
			Player.DescribeScreen(Open, Words, Hand);
			const FString Shown = Open + TEXT("|") + Words + TEXT("|") + Hand;
			if (Shown != ShownLast)
			{
				LastMs = ActMs + SinceAct * 1000.0;
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
		// Let it happen: a march is flown across the table, a month is played through, a card opens.
		const float Settle = bMonth ? 2.5f : 1.1f;
		if (Clock < Settle || (Player.IsBusyForSteps() && Clock < (bMonth ? 120.0f : 20.0f)))
		{
			if (bMonth && Clock > 1.0f)
			{
				Player.StepThroughMonth(); // (the questions and the screens a month's end brings are answered as a player in a hurry would)
			}
			return true;
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
