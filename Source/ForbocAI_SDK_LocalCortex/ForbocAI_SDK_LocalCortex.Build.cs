using UnrealBuildTool;

public class ForbocAI_SDK_LocalCortex : ModuleRules
{
	public ForbocAI_SDK_LocalCortex(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"ForbocAI_SDK"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"HTTP",
				"Json",
				"JsonUtilities"
			}
		);

		// Disable native Llama by default for cross-platform stability until binaries are included.
		PublicDefinitions.Add("WITH_FORBOC_NATIVE=0");
	}
}