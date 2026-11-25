#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Campaign/CampaignPlayerController.h"
#include "Editor.h"
#include "HAL/PlatformTime.h"
#include "Misc/App.h"
#include "PlayInEditorDataTypes.h"
#include "Tests/AutomationEditorCommon.h"
#include "TheaterPlaytestSubsystem.h"
#include "TheaterPlaytestToolset.h"
#include "ToolsetRegistry/ToolCallExceptionHandler.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
	constexpr const TCHAR* CampaignMap = TEXT("/Game/Campaign/Maps/CampaignMap");
	constexpr const TCHAR* StartWar = TEXT("UK 1939-09 seed 4");

	/** What the stages of one play-session test share. */
	struct FChain
	{
		TStrongObjectPtr<UTheaterStepAsyncResult> Step;
		double Since = 0.0;
	};

	/** Whether this run can open a play session; says why not where it cannot. */
	bool CanPlay(FAutomationTestBase& Test)
	{
		if (!FApp::CanEverRender())
		{
			Test.AddInfo(TEXT("Skipped: this run draws nothing (-NullRHI), so no play session can run."));
			return false;
		}
		if (GEditor->PlayWorld)
		{
			Test.AddInfo(TEXT("Skipped: a play session is already running."));
			return false;
		}
		return true;
	}

	/** Opens the campaign map, starts a play session of the given kind on it, and waits until the campaign waits on the player. */
	void QueueSession(FAutomationTestBase& Test, TSharedRef<FChain> Chain, EPlaySessionWorldType Kind = EPlaySessionWorldType::PlayInEditor)
	{
		Test.AddCommand(new FFunctionLatentCommand([Chain, Kind]()
		{
			FAutomationEditorCommonUtils::LoadMap(CampaignMap);
			FRequestPlaySessionParams Params;
			Params.WorldType = Kind;
			GEditor->RequestPlaySession(Params);
			Chain->Since = FPlatformTime::Seconds();
			return true;
		}));
		Test.AddCommand(new FFunctionLatentCommand([&Test, Chain, Kind]()
		{
			if (Kind == EPlaySessionWorldType::SimulateInEditor)
			{
				if (GEditor->PlayWorld)
				{
					return true;
				}
				if (FPlatformTime::Seconds() - Chain->Since > 180.0)
				{
					Test.AddError(TEXT("The simulation did not start within 3 minutes."));
					return true;
				}
				return false;
			}
			FString Error;
			const ACampaignPlayerController* Player = UTheaterPlaytestSubsystem::FindPlayer(Error);
			if (Player && !Player->IsBusyForSteps())
			{
				return true;
			}
			if (FPlatformTime::Seconds() - Chain->Since > 180.0)
			{
				Test.AddError(FString::Printf(TEXT("The campaign did not come up within 3 minutes: %s"), *Error));
				return true;
			}
			return false;
		}));
	}

	/** Plays a step, waits for its result, and hands the result to Check. */
	void QueueStep(FAutomationTestBase& Test, TSharedRef<FChain> Chain, ETheaterStepVerb Verb, const FString& Target, bool bPicture,
		TFunction<void(const UTheaterStepAsyncResult&)> Check)
	{
		Test.AddCommand(new FFunctionLatentCommand([Chain, Verb, Target, bPicture]()
		{
			Chain->Step.Reset(UTheaterPlaytestToolset::PlayStep(Verb, Target, bPicture));
			Chain->Since = FPlatformTime::Seconds();
			return true;
		}));
		Test.AddCommand(new FFunctionLatentCommand([&Test, Chain, Check]()
		{
			if (!Chain->Step->bIsComplete)
			{
				if (FPlatformTime::Seconds() - Chain->Since < 1200.0)
				{
					return false;
				}
				Test.AddError(TEXT("The step did not finish within 20 minutes."));
				return true;
			}
			Check(*Chain->Step);
			return true;
		}));
	}

	/** Ends the play session and waits until it has gone. */
	void QueueEndSession(FAutomationTestBase& Test)
	{
		Test.AddCommand(new FFunctionLatentCommand([]()
		{
			if (GEditor->PlayWorld)
			{
				GEditor->RequestEndPlayMap();
			}
			return true;
		}));
		Test.AddCommand(new FFunctionLatentCommand([]() { return GEditor->PlayWorld == nullptr; }));
	}
}

BEGIN_DEFINE_SPEC(FTheaterPlaytestToolsetSpec, "TheaterCommand.Editor.PlaytestToolset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FTheaterPlaytestToolsetSpec)

void FTheaterPlaytestToolsetSpec::Define()
{
	Describe(TEXT("GetScreen"), [this]()
	{
		It(TEXT("raises when no play session is running"), [this]()
		{
			if (GEditor->PlayWorld)
			{
				AddInfo(TEXT("Skipped: a play session is already running."));
				return;
			}
			UE::ToolsetRegistry::FToolCallExceptionHandler Handler;
			Handler.CaptureErrorsIn([]() { UTheaterPlaytestToolset::GetScreen(); });
			TestTrue(TEXT("Raised: no play session"), Handler.GetException().Contains(TEXT("No play session is running")));
		});

		It(TEXT("raises when the play session has no campaign player"), [this]()
		{
			if (!CanPlay(*this))
			{
				return;
			}
			const TSharedRef<FChain> Chain = MakeShared<FChain>();
			QueueSession(*this, Chain, EPlaySessionWorldType::SimulateInEditor);
			AddCommand(new FFunctionLatentCommand([this]()
			{
				UE::ToolsetRegistry::FToolCallExceptionHandler Handler;
				Handler.CaptureErrorsIn([]() { UTheaterPlaytestToolset::GetScreen(); });
				TestTrue(TEXT("Raised: not the campaign"), Handler.GetException().Contains(TEXT("not playing the campaign map")));
				return true;
			}));
			QueueEndSession(*this);
		});

		It(TEXT("reads the opening screen of the campaign"), [this]()
		{
			if (!CanPlay(*this))
			{
				return;
			}
			const TSharedRef<FChain> Chain = MakeShared<FChain>();
			QueueSession(*this, Chain);
			AddCommand(new FFunctionLatentCommand([this]()
			{
				UE::ToolsetRegistry::FToolCallExceptionHandler Handler;
				FTheaterScreen Screen;
				Handler.CaptureErrorsIn([&Screen]() { Screen = UTheaterPlaytestToolset::GetScreen(); });
				TestTrue(TEXT("Nothing raised"), Handler.GetException().IsEmpty());
				TestEqual(TEXT("The opening screen is on top"), Screen.Open, FString(TEXT("The opening screen")));
				TestFalse(TEXT("No battle covers the map"), Screen.bInBattle);
				return true;
			}));
			QueueEndSession(*this);
		});
	});

	Describe(TEXT("PlayStep"), [this]()
	{
		It(TEXT("fails when no play session is running"), [this]()
		{
			if (GEditor->PlayWorld)
			{
				AddInfo(TEXT("Skipped: a play session is already running."));
				return;
			}
			const UTheaterStepAsyncResult* Result = UTheaterPlaytestToolset::PlayStep(ETheaterStepVerb::Press, TEXT("CONTINUE"));
			TestTrue(TEXT("Complete at once"), Result->bIsComplete);
			TestTrue(TEXT("Error: no play session"), Result->Error.Contains(TEXT("No play session is running")));
		});

		It(TEXT("fails when the target is empty"), [this]()
		{
			const UTheaterStepAsyncResult* Result = UTheaterPlaytestToolset::PlayStep(ETheaterStepVerb::Press, TEXT("  "));
			TestTrue(TEXT("Complete at once"), Result->bIsComplete);
			TestTrue(TEXT("Error: empty target"), Result->Error.Contains(TEXT("Target must not be empty")));
		});

		It(TEXT("starts a war, judges a press of words no button carries, and pictures a step"), [this]()
		{
			if (!CanPlay(*this))
			{
				return;
			}
			const TSharedRef<FChain> Chain = MakeShared<FChain>();
			QueueSession(*this, Chain);
			QueueStep(*this, Chain, ETheaterStepVerb::Start, StartWar, false, [this](const UTheaterStepAsyncResult& Result)
			{
				TestTrue(FString::Printf(TEXT("No error: %s"), *Result.Error), Result.Error.IsEmpty());
				TestTrue(FString::Printf(TEXT("The war began: %s"), *UEnum::GetValueAsString(Result.Value.Verdict)), Result.Value.Verdict == ETheaterStepVerdict::Did);
				TestNotEqual(TEXT("The opening screen is gone"), Result.Value.Screen.Open, FString(TEXT("The opening screen")));
				TestTrue(TEXT("Frames were timed"), Result.Value.MedianFps > 0.0f);
				TestTrue(TEXT("No picture was asked for"), Result.Value.Picture.Data.IsEmpty());
			});
			QueueStep(*this, Chain, ETheaterStepVerb::Press, TEXT("NO BUTTON CARRIES THESE WORDS"), false, [this](const UTheaterStepAsyncResult& Result)
			{
				TestTrue(FString::Printf(TEXT("No error: %s"), *Result.Error), Result.Error.IsEmpty());
				TestTrue(FString::Printf(TEXT("Judged missing: %s"), *UEnum::GetValueAsString(Result.Value.Verdict)), Result.Value.Verdict == ETheaterStepVerdict::Missing);
				TestTrue(TEXT("The note says why"), Result.Value.Note.Contains(TEXT("no button")));
			});
			QueueStep(*this, Chain, ETheaterStepVerb::Key, TEXT("tab"), true, [this](const UTheaterStepAsyncResult& Result)
			{
				TestTrue(FString::Printf(TEXT("No error: %s"), *Result.Error), Result.Error.IsEmpty());
				TestEqual(TEXT("A PNG picture"), Result.Value.Picture.MimeType, FString(TEXT("image/png")));
				TestFalse(TEXT("The picture holds data"), Result.Value.Picture.Data.IsEmpty());
			});
			QueueEndSession(*this);
		});

		It(TEXT("fails a second step while one plays"), [this]()
		{
			if (!CanPlay(*this))
			{
				return;
			}
			const TSharedRef<FChain> Chain = MakeShared<FChain>();
			QueueSession(*this, Chain);
			QueueStep(*this, Chain, ETheaterStepVerb::Start, StartWar, false, [](const UTheaterStepAsyncResult&) {});
			AddCommand(new FFunctionLatentCommand([this, Chain]()
			{
				Chain->Step.Reset(UTheaterPlaytestToolset::PlayStep(ETheaterStepVerb::EndMonth, FString()));
				Chain->Since = FPlatformTime::Seconds();
				const UTheaterStepAsyncResult* Second = UTheaterPlaytestToolset::PlayStep(ETheaterStepVerb::Close, FString());
				TestTrue(TEXT("The second is complete at once"), Second->bIsComplete);
				TestTrue(TEXT("Error: already playing"), Second->Error.Contains(TEXT("already playing")));
				return true;
			}));
			AddCommand(new FFunctionLatentCommand([this, Chain]()
			{
				if (!Chain->Step->bIsComplete && FPlatformTime::Seconds() - Chain->Since < 300.0)
				{
					return false;
				}
				TestTrue(FString::Printf(TEXT("The first finished without error: %s"), *Chain->Step->Error), Chain->Step->bIsComplete && Chain->Step->Error.IsEmpty());
				TestTrue(FString::Printf(TEXT("The month ended: %s"), *UEnum::GetValueAsString(Chain->Step->Value.Verdict)), Chain->Step->Value.Verdict == ETheaterStepVerdict::Did);
				return true;
			}));
			QueueEndSession(*this);
		});

		It(TEXT("fails a step whose play session ends under it"), [this]()
		{
			if (!CanPlay(*this))
			{
				return;
			}
			const TSharedRef<FChain> Chain = MakeShared<FChain>();
			QueueSession(*this, Chain);
			QueueStep(*this, Chain, ETheaterStepVerb::Start, StartWar, false, [](const UTheaterStepAsyncResult&) {});
			AddCommand(new FFunctionLatentCommand([Chain]()
			{
				Chain->Step.Reset(UTheaterPlaytestToolset::PlayStep(ETheaterStepVerb::EndMonth, FString()));
				GEditor->RequestEndPlayMap();
				return true;
			}));
			AddCommand(new FFunctionLatentCommand([this, Chain]()
			{
				if (GEditor->PlayWorld || !Chain->Step->bIsComplete)
				{
					return false;
				}
				TestTrue(TEXT("Error: the session ended"), Chain->Step->Error.Contains(TEXT("play session ended")));
				return true;
			}));
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
