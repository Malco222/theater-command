#include "Campaign/CampaignSession.h"

#include "Campaign/TheaterSpeed.h"
#include "Rules/RulesHost.h"
#include "TheaterCommand.h"
#include "Dom/JsonObject.h"

/**
 * (Release 2: a neutral's bargains and the volunteer legions on the campaign map. Build contract
 * production/r2-bargains-legions/contract.md, D.4.)
 *
 * The session's three calls for them (Rules/bargains-look.ts): the map's look at the bargains and the legions, which
 * changes nothing, and the player's two orders, a bid and a sale. Both rules are switched off in release 1 (stage.json
 * 'bargains' and 'legions'); then the look answers exactly {"on":false}, the map draws nothing of them, and nothing
 * gives either order.
 */

namespace
{
	/** Text as a JSON string, quotes and all (CampaignSession.cpp's JsonQuote, which is that file's own). */
	FString BargainQuote(const FString& Text)
	{
		FString Out;
		Out.Reserve(Text.Len() + 2);
		Out.AppendChar(TEXT('"'));
		for (const TCHAR Ch : Text)
		{
			if (Ch == TEXT('"') || Ch == TEXT('\\'))
			{
				Out.AppendChar(TEXT('\\'));
				Out.AppendChar(Ch);
			}
			else if (Ch < 0x20)
			{
				Out += FString::Printf(TEXT("\\u%04x"), static_cast<int32>(Ch));
			}
			else
			{
				Out.AppendChar(Ch);
			}
		}
		Out.AppendChar(TEXT('"'));
		return Out;
	}
}

bool FCampaignSession::BargainsLook(FString& OutJson)
{
	OutJson.Reset();
	// As CallRules asks a question that changes nothing ("bargainsLook" is one of IsQuestion's): an order still being
	// answered comes first, and the answer is kept until something is asked that may change the war. It never forgets
	// the questions kept, and the answer is handed back as text for Campaign/BargainFeed's own reader.
	Settle();
	if (!bRunning)
	{
		return false;
	}
	THEATER_SPEED_SCOPE(Rules);
	const FString Key = TheaterSpeed::RulesMemo() ? FString(TEXT("bargainsLook|[]")) : FString();
	if (const FString* Known = Key.IsEmpty() ? nullptr : RulesMemo.Find(Key))
	{
		OutJson = *Known;
		TheaterSpeed::NoteRulesCall(TEXT("bargainsLook"), 0.0, true);
		return true;
	}
	const double Began = FPlatformTime::Seconds();
	FString Error;
	if (!Host->Call(TEXT("bargainsLook"), TEXT("[]"), OutJson, Error))
	{
		// (rules without the call, the stand-in's: the map shows none of it; said once)
		static bool bSaid = false;
		if (!bSaid)
		{
			bSaid = true;
			UE_LOG(LogTheater, Display, TEXT("Rules: no bargains look (%s)"), *Error);
		}
		OutJson.Reset();
		return false;
	}
	if (!Key.IsEmpty())
	{
		if (RulesMemo.Num() >= 256)
		{
			RulesMemo.Reset(); // (as CallRules: a long month of looking about)
		}
		RulesMemo.Add(Key, OutJson);
	}
	TheaterSpeed::NoteRulesCall(TEXT("bargainsLook"), FPlatformTime::Seconds() - Began, false);
	return true;
}

bool FCampaignSession::BargainBid(const FString& Id, double Pp, FString& OutNote)
{
	// (an order, as Court: the rules' answer with its note and the new picture of the war)
	TSharedPtr<FJsonObject> Object;
	if (!CallRules(TEXT("bargainBid"), FString::Printf(TEXT("[%s,%s]"), *BargainQuote(Id), *FString::SanitizeFloat(Pp)), Object))
	{
		return false;
	}
	bool bOk = false;
	Object->TryGetBoolField(TEXT("ok"), bOk);
	Object->TryGetStringField(TEXT("note"), OutNote);
	TakeView(Object);
	return bOk;
}

bool FCampaignSession::BargainSell(const FString& Id, const FString& Buyer, FString& OutNote)
{
	TSharedPtr<FJsonObject> Object;
	if (!CallRules(TEXT("bargainSell"), FString::Printf(TEXT("[%s,%s]"), *BargainQuote(Id), Buyer.IsEmpty() ? TEXT("null") : *BargainQuote(Buyer)), Object))
	{
		return false;
	}
	bool bOk = false;
	Object->TryGetBoolField(TEXT("ok"), bOk);
	Object->TryGetStringField(TEXT("note"), OutNote);
	TakeView(Object);
	return bOk;
}
