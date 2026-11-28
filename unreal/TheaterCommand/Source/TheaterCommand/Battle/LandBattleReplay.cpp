// The replay map after a land battle (release 2, -TheaterReplayMap; LandReplay.h): ALandBattle's part. Once the after
// card is up it lays the map from the battle's own recording (every view the rules sent), plays its 20 real seconds
// while it is open, and takes the after card's REPLAY MAP and the map's AGAIN and CLOSE. SLandBattleScreen draws it.

#include "Battle/LandBattle.h"

#include "Battle/LandBattleState.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "TheaterCommand.h"

namespace
{
	bool ReplayMapOn()
	{
		return FParse::Param(FCommandLine::Get(), TEXT("TheaterReplayMap"));
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
		if (S.ReplayLaid > 0)
		{
			size_t Moved = 0;
			for (const LandReplay::Piece& P : S.ReplayMap.Pieces)
			{
				if (LandReplay::Length(LandReplay::WayTo(P, S.ReplayMap.T1)) >= LandReplay::LeastMove)
				{
					++Moved;
				}
			}
			UE_LOG(LogTheater, Display, TEXT("Replay map: laid, %d units (%d moved far enough for an arrow), %d patches of ground, %.0f s of battle"),
				static_cast<int32>(S.ReplayMap.Pieces.size()), static_cast<int32>(Moved), static_cast<int32>(S.ReplayMap.Patches.size()), S.ReplayMap.T1 - S.ReplayMap.T0);
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
