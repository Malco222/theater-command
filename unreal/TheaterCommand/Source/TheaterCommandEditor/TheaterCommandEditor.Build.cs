using UnrealBuildTool;

/**
 * The editor's side of the game (release 2, Brian 25 November 2026: Claude play-tests the game inside the editor). An
 * Editor module: the Play icon runs "UnrealEditor -game", which never loads it, so the game he plays is unchanged.
 */
public class TheaterCommandEditor : ModuleRules
{
	public TheaterCommandEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Each file is compiled by itself, as the game's module is.
		bUseUnity = false;

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"EditorSubsystem",
			"ImageWrapper",
			"Slate",
			"SlateCore",
			"UnrealEd",
			// Unreal's registry of the tools Claude can call through the editor's MCP link.
			"ToolsetRegistry",
			// The game itself: its campaign player and the playtester's step player.
			"TheaterCommand"
		});
	}
}
