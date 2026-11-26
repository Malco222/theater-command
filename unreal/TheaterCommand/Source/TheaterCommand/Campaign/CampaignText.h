#pragma once

#include "CoreMinimal.h"

namespace CampaignText
{
	/**
	 * The staff's band of odds in words (the UI director, 18 November 2026: "between 3.0 to 1 for you and 3.9 to 1 for
	 * you" reads "between 3.0 and 3.9 to 1 for you"). Both ends on one side share its words; a band that straddles
	 * even odds names each end.
	 */
	inline FString OddsBetween(float Low, float High)
	{
		auto Against = [](float Odds) { return 1.0f / FMath::Max(Odds, 0.01f); };
		if (Low >= 1.0f && High >= 1.0f)
		{
			return FString::Printf(TEXT("between %.1f and %.1f to 1 for you"), Low, High);
		}
		if (Low < 1.0f && High < 1.0f)
		{
			// (the worse end first, as the band is given)
			return FString::Printf(TEXT("between %.1f and %.1f to 1 against"), Against(Low), Against(High));
		}
		return FString::Printf(TEXT("between %.1f to 1 against and %.1f to 1 for you"), Against(Low), High);
	}

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
