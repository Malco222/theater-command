#pragma once

#include <string>

/**
 * (Release 2: the battle schools; Brian approved them on 22 November.) The button on a corps' card that sends it to
 * the battle schools for the month, or brings it back while it is there: whether the card shows it at all, which of
 * its two faces, whether it is lit, and what its tip says.
 *
 * The rules decide everything (TC.school, and each corps' school, schoolTo and schoolWhy in the view); this only reads
 * what they say. Where it cannot be pressed, its tip is the rules' own reason, word for word. Release 1's rules have
 * no schools: their view says nothing of them, and then no button shows at all.
 *
 * Plain C++, no engine: Tools/school/run.sh builds and checks it in the cloud.
 */
namespace BattleSchool
{
	/** What a corps' card knows of the battle schools, as the rules' view gives it. */
	struct FCorpsFacts
	{
		bool bKnown = false;    // the view says 'school' at all for this corps (release 2's rules); release 1's never does
		bool bOwn = false;      // a corps of the player's own country (not an ally's, marching with his army)
		bool bGrand = false;    // the grand campaign (the view's 'open'), the only war the schools are open in
		bool bFort = false;     // fortress troops: they never leave their works, and their card says so already
		bool bHisMonth = false; // the player's month: an order can be given now
		bool bAtSchool = false; // at the schools this month
		std::string To;         // the grade a month there brings it to ("Veteran"); may be empty
		std::string Why;        // the rules' sentence why it cannot go; empty if it can
	};

	enum class EFace
	{
		None,
		Send,
		Back
	};

	struct FButton
	{
		EFace Face = EFace::None;
		bool bLit = false;
		std::string Label; // "SEND TO SCHOOL" or "BRING BACK"; empty when no button shows
		std::string Tip;   // unlit: the rules' reason, word for word; lit: what a press does
	};

	/** The card's school button for a corps, from what the view says of it. */
	FButton ButtonFor(const FCorpsFacts& Corps);

	/** A grade as it is written inside a sentence: "Veteran" becomes "veteran". Letters outside plain ASCII are left as they are. */
	std::string GradeInSentence(const std::string& Grade);
}
