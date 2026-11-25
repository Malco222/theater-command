using UnrealBuildTool;
using System.Collections.Generic;

public class TheaterCommandEditorTarget : TargetRules
{
	public TheaterCommandEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("TheaterCommand");
		ExtraModuleNames.Add("TheaterCommandEditor"); // (the editor's side: Claude's playtest tools)
	}
}
