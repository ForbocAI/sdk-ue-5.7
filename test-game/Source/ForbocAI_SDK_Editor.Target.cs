using UnrealBuildTool;
using System.Collections.Generic;

public class ForbocAI_SDK_EditorTarget : TargetRules
{
	public ForbocAI_SDK_EditorTarget( TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V6;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_7;

		ExtraModuleNames.AddRange( new string[] { "ForbocAI_SDK" } );
	}
}
