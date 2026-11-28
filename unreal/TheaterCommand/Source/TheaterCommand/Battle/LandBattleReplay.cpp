// The replay map after a land battle (release 2, -TheaterReplayMap; LandReplay.h): ALandBattle's part. Once the after
// card is up it lays the map from the battle's own recording (every view the rules sent), plays its 20 real seconds
// while it is open, and takes the after card's REPLAY MAP and the map's AGAIN and CLOSE. SLandBattleScreen draws it.

#include "Battle/LandBattle.h"

#include "Battle/LandBattleState.h"
#include "Battle/LandField.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "TheaterCommand.h"

namespace
{
	/** The ship list's row (read once: the screen asks every frame). */
	bool ReplayMapOn()
	{
		static const bool bOn = FParse::Param(FCommandLine::Get(), TEXT("TheaterReplayMap"));
		return bOn;
	}
}

void ALandBattle::TickReplay(double Real)
{
	if (!State.IsValid())
	{
		return;
	}
	FLandBattleState& S = *State;
	if (!S.bCard)
	{
		return;
	}
	if (S.ReplayLaid == 0 && ReplayMapOn())
	{
		std::string Why;
		S.ReplayLaid = LandReplay::Build(S.Rec, S.ReplayMap, Why) ? 1 : -1;
		if (S.ReplayLaid > 0 && S.Field)
		{
			// (the hedgerows, walls and lanes the scene laid: a lane's bank and a garden's hedge drawn as hedgerows; a
			// wood's edge is its patch's, and the far strips lie beyond the field)
			for (const LandField::HedgeLine& H : S.Field->Hedges())
			{
				if (H.Kind == 0 || H.Kind == 1 || H.Kind == 3)
				{
					LandReplay::AddLine(S.ReplayMap, H.Points, LandReplay::Edge::Hedge);
				}
				else if (H.Kind == 4)
				{
					LandReplay::AddLine(S.ReplayMap, H.Points, LandReplay::Edge::Wall);
				}
			}
			for (const std::vector<std::pair<double, double>>& Lane : S.Field->Lanes())
			{
				LandReplay::AddLine(S.ReplayMap, Lane, LandReplay::Edge::Lane);
			}
		}
		if (S.ReplayLaid > 0)
		{
			size_t Moved = 0, Fled = 0, FellBack = 0;
			for (const LandReplay::Piece& P : S.ReplayMap.Pieces)
			{
				if (LandReplay::Length(LandReplay::WayTo(P, S.ReplayMap.T1)) >= LandReplay::LeastMove)
				{
					++Moved;
				}
				const LandReplay::Pose End = LandReplay::PoseAt(P, S.ReplayMap.T1);
				Fled += End.bGone ? 1 : 0;
				FellBack += End.bOff ? 1 : 0;
			}
			size_t Points = 0;
			for (const LandReplay::Line& L : S.ReplayMap.Lines)
			{
				Points += L.Points.size();
			}
			UE_LOG(LogTheater, Display, TEXT("Replay map: laid, %d units (%d moved far enough for an arrow, %d fled, %d fell back), %d patches of ground, %d hedgerows, walls and lanes (%d points), %.0f s of battle"),
				static_cast<int32>(S.ReplayMap.Pieces.size()), static_cast<int32>(Moved), static_cast<int32>(Fled), static_cast<int32>(FellBack), static_cast<int32>(S.ReplayMap.Patches.size()),
				static_cast<int32>(S.ReplayMap.Lines.size()), static_cast<int32>(Points), S.ReplayMap.T1 - S.ReplayMap.T0);
		}
		else
		{
			UE_LOG(LogTheater, Warning, TEXT("Replay map: not laid (%s): no REPLAY MAP on the after card"), UTF8_TO_TCHAR(Why.c_str()));
		}
	}
	S.Replay.Tick(Real);
}

bool ALandBattle::ReplayShown() const
{
	return State.IsValid() && State->bCard && State->ReplayLaid > 0 && ReplayMapOn();
}

void ALandBattle::ReplayDo(const FString& Word)
{
	if (!ReplayShown())
	{
		return;
	}
	FLandBattleState& S = *State;
	if (Word == TEXT("open"))
	{
		S.Replay.Open();
		UE_LOG(LogTheater, Display, TEXT("Replay map: opened"));
	}
	else if (Word == TEXT("again"))
	{
		if (S.Replay.IsOpen())
		{
			S.Replay.Again();
			UE_LOG(LogTheater, Display, TEXT("Replay map: again from the start"));
		}
	}
	else if (Word == TEXT("close"))
	{
		if (S.Replay.IsOpen())
		{
			S.Replay.Close();
			UE_LOG(LogTheater, Display, TEXT("Replay map: closed, back to the after card"));
		}
	}
}
