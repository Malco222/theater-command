using UnrealBuildTool;

public class TheaterCommand : ModuleRules
{
	public TheaterCommand(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Lets files include each other by their path inside the module, e.g. "Campaign/CampaignMapData.h".
		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"Json",
			"ProceduralMeshComponent",
			"GeometryCore",
			"Slate",
			"SlateCore",
			"UMG"
		});

		// Only for the development photographer, which reports how long a frame takes to draw.
		PrivateDependencyModuleNames.AddRange(new string[] { "RenderCore", "RHI" });
	}
}
