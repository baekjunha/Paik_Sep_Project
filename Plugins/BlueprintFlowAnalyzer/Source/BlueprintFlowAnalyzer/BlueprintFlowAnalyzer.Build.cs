using UnrealBuildTool;

public class BlueprintFlowAnalyzer : ModuleRules
{
    public BlueprintFlowAnalyzer(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Core", "CoreUObject", "Engine", "AssetRegistry", "Json", "Projects", "BlueprintGraph"
        });
    }
}
