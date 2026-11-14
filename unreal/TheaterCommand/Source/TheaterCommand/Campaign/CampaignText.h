#pragma once

#include "CoreMinimal.h"

namespace CampaignText
{
	/**
	 * A line of phrases parted by separator dots ("XIX Corps  ·  armoured, 2 divisions  ·  30,000 men"), set so that
	 * where it has to be folded it breaks only after a dot and never inside a phrase (the art director's ruling on
	 * the typewriter face: no word left alone on a line, and no line that starts with a dot). Every space becomes
	 * one that holds its words together, except the last space after each dot.
	 */
	inline FString Phrases(const FString& Line)
	{
		static const TCHAR Hold = TEXT(' '), Dot = TEXT('·');
		FString Out = Line;
		for (int32 At = 0; At < Out.Len(); ++At)
		{
			if (Out[At] == TEXT(' '))
			{
				Out[At] = Hold;
			}
		}
		for (int32 At = 0; At < Out.Len(); ++At)
		{
			if (Out[At] != Dot)
			{
				continue;
			}
			int32 Last = At;
			while (Last + 1 < Out.Len() && Out[Last + 1] == Hold)
			{
				++Last;
			}
			if (Last > At)
			{
				Out[Last] = TEXT(' ');
			}
		}
		return Out;
	}
}
