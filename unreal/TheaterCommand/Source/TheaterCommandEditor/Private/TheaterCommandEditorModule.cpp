#include "Modules/ModuleManager.h"
#include "ToolsetRegistry/UToolsetRegistry.h"
#include "TheaterPlaytestToolset.h"

/** The editor's side of the game: it puts the game's own tools in Unreal's toolset registry. */
class FTheaterCommandEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		UToolsetRegistry::RegisterToolsetClass(UTheaterPlaytestToolset::StaticClass());
	}

	virtual void ShutdownModule() override
	{
		UToolsetRegistry::UnregisterToolsetClass(UTheaterPlaytestToolset::StaticClass());
	}
};

IMPLEMENT_MODULE(FTheaterCommandEditorModule, TheaterCommandEditor)
