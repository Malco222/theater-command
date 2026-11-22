using UnrealBuildTool;

public class TheaterCommand : ModuleRules
{
	public TheaterCommand(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Each file is compiled by itself. Unreal otherwise glues a module's files together once there are 32 of
		// them, and thirteen small helpers that several files each keep for themselves (FromHex, White, Brass and
		// the like) would then clash.
		bUseUnity = false;

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

		// The game's rules are JavaScript, run by the JavaScript engine that comes with macOS.
		if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			PublicFrameworks.Add("JavaScriptCore");
		}

		// Only for the development photographer, which reports how long a frame takes to draw.
		PrivateDependencyModuleNames.AddRange(new string[] { "RenderCore", "RHI" });

		// The air battle's master limiter (mix.json buses.master.limiterCeilingDb): the engine's own submix limiter.
		PrivateDependencyModuleNames.Add("AudioMixer");

		// The air battle's aircraft copies (AS1): meshes made while the game runs, for instancing. (The ground battle's
		// patch adds the same two for its hedges, trees, grass and crowds: keep one line when the two meet.)
		foreach (string Module in new string[] { "MeshDescription", "StaticMeshDescription" })
		{
			if (!PrivateDependencyModuleNames.Contains(Module))
			{
				PrivateDependencyModuleNames.Add(Module);
			}
		}
	}
}
