// Solstice - game target (M0).
// See docs/TECH.md SS2 / SS3 for the target vs module distinction.

using UnrealBuildTool;
using System.Collections.Generic;

public class SolsticeTarget : TargetRules
{
	public SolsticeTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.AddRange(new string[] { "Solstice" });
	}
}
