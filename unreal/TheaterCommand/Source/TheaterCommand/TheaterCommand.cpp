#include "TheaterCommand.h"
#include "Modules/ModuleManager.h"
#include "Release2Ship.h"
#include "Look/WholeLook.h"

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
		// (the whole picture's looks, after the list has set their variables: Look/WholeLook.h)
		WholeLook::Start();
	}

	virtual void ShutdownModule() override
	{
		WholeLook::Stop();
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE(FTheaterCommandModule, TheaterCommand, "TheaterCommand");
