#include "Campaign/BattleSchool.h"

namespace BattleSchool
{
	std::string GradeInSentence(const std::string& Grade)
	{
		std::string Out = Grade;
		for (char& Letter : Out)
		{
			if (Letter >= 'A' && Letter <= 'Z')
			{
				Letter = static_cast<char>(Letter - 'A' + 'a');
			}
		}
		return Out;
	}

	FButton ButtonFor(const FCorpsFacts& Corps)
	{
		FButton Out;
		// Nothing at all where the rules have no schools (release 1), outside the grand campaign, on an ally's corps, on
		// fortress troops, or outside the player's month: there is no order to give there, and no button says so.
		if (!Corps.bKnown || !Corps.bGrand || !Corps.bOwn || Corps.bFort || !Corps.bHisMonth)
		{
			return Out;
		}
		const std::string Grade = GradeInSentence(Corps.To);
		if (Corps.bAtSchool)
		{
			// The rules let a corps be called back before the month is out: it is ready for orders, and has had no term.
			Out.Face = EFace::Back;
			Out.bLit = true;
			Out.Label = "BRING BACK";
			Out.Tip = (Grade.empty()
				? std::string("At the battle schools this month: left in peace, it comes back a grade better when your next month opens.")
				: "At the battle schools this month: left in peace, it comes back " + Grade + " when your next month opens.")
				+ " Called back now, it is ready for orders, and has had no term.";
			return Out;
		}
		Out.Face = EFace::Send;
		Out.Label = "SEND TO SCHOOL";
		if (!Corps.Why.empty())
		{
			Out.Tip = Corps.Why; // (the rules' own reason, word for word)
			return Out;
		}
		Out.bLit = true;
		Out.Tip = (Grade.empty()
			? std::string("A quiet month at the battle schools at home: left in peace, it comes back a grade better when your next month opens.")
			: "A quiet month at the battle schools at home: left in peace, it comes back " + Grade + " when your next month opens.")
			+ " It takes no orders while it is there, and if it has to fight the term is lost.";
		return Out;
	}
}
