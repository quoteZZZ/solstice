// Solstice - editor module build rules.
// Hosts the commandlets that generate materials and run the headless M0 regression.

using UnrealBuildTool;

public class SolsticeEditor : ModuleRules
{
	public SolsticeEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"Solstice"
		});

		PublicIncludePaths.Add(ModuleDirectory);

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"UnrealEd",
			"MaterialEditor",
			"AssetTools",
			"AssetRegistry",
			"RenderCore",
			"RHI"
		});
	}
}
