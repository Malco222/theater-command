#include "TheaterCommand.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreMisc.h"
#include "Misc/Parse.h"
#include "Modules/ModuleManager.h"
#include "Release2Ship.h"
#include "Look/WholeLook.h"

DEFINE_LOG_CATEGORY(LogTheater);

namespace
{
	/**
	 * The Play icon runs the editor's own build as a game (-game), and the editor's Python wakes there too. The plugins of
	 * Claude's link to the editor (ToolsetRegistry and its toolsets) then run their start-up Python scripts, which need
	 * classes only the full editor has, and fill the game's log with errors. The game never uses Python, so a -game run
	 * leaves it off. Python reads this word only after the engine has started (OnPostEngineInit), well after this module
	 * starts. The full editor, and the command-line tools that run Python (Tools/build_content.sh), keep it; a run that
	 * does want it in the game can say -ForceEnablePython, which wins over this word.
	 */
	void KeepPythonOutOfTheGame()
	{
#if WITH_EDITOR
		if (IsRunningGame() && !FParse::Param(FCommandLine::Get(), TEXT("DisablePython")))
		{
			FCommandLine::Append(TEXT(" -DisablePython"));
		}
#endif
	}
}

/**
 * The game's module. At its start the release 2 ship list puts in the launch words of every release 2 feature that
 * ships on (Brian, 25 November 2026: "turn them on automatically"; Release2Ship.h), before anything reads them.
 */
class FTheaterCommandModule : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
		KeepPythonOutOfTheGame();
		Release2Ship::Apply();
		// (the whole picture's looks, after the list has set their variables: Look/WholeLook.h)
		WholeLook::Start();
	}

	virtual void ShutdownModule() override
	{
		WholeLook::Stop();
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE(FTheaterCommandModule, TheaterCommand, "TheaterCommand");
