// Solstice - runtime module build rules.
//
// Layering enforced by convention inside this module (docs/tech/ARCHITECTURE.md SS2.1):
//   Render -> World -> Sim
// Sim/ must stay free of UObject / Tick / render / input dependencies so it can be driven
// headless. Dependencies kept minimal: Slate/HTTP private, no UMG/RenderCore/RHI.

using UnrealBuildTool;

public class Solstice : ModuleRules
{
	public Solstice(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"Json",
			"ProceduralMeshComponent"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore",
			"HTTP"
		});

		// Allow headers to be included as "Sim/SolsticeKernel.h" etc.
		PublicIncludePaths.Add(ModuleDirectory);
	}
}
