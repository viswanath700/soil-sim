using System.IO;
using UnrealBuildTool;

public class SoilSimViewer : ModuleRules
{
    public SoilSimViewer(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        bEnableExceptions = true;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "ProceduralMeshComponent"
            });

        string projectRoot = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", "..", ".."));
        string coreInclude = Path.Combine(projectRoot, "core", "include");
        string coreLibrary = Path.Combine(
            projectRoot, "out", "build", "release", "core", "soilsim_core.lib");

        PublicIncludePaths.Add(coreInclude);
        PublicAdditionalLibraries.Add(coreLibrary);
        PublicDefinitions.Add("SOILSIM_VIEWER_REQUIRES_RELEASE_CORE=1");
    }
}
