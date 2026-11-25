#include "TheaterCommand.h"
#include "Modules/ModuleManager.h"
#include "Release2Ship.h"

DEFINE_LOG_CATEGORY(LogTheater);

/**
 * The game's module. At its start the release 2 ship list puts in the launch words of every release 2 feature that
 * ships on (Brian, 25 November 2026: "turn them on automatically"; Release2Ship.h), before anything reads them.
 */
class FTheaterCommandModule : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
		Release2Ship::Apply();
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE(FTheaterCommandModule, TheaterCommand, "TheaterCommand");
