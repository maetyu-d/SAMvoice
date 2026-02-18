using UnrealBuildTool;

public class SAMUdpMetaSound : ModuleRules
{
    public SAMUdpMetaSound(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "MetasoundEngine",
            "MetasoundFrontend",
            "MetasoundGraphCore",
            "MetasoundStandardNodes",
            "Sockets",
            "Networking"
        });
    }
}
