// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SlateAgentBridge : ModuleRules
{
    public SlateAgentBridge(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "Json",
                "JsonUtilities",
                "HTTPServer",
                "UnrealEd",
                "HotReload",
                "Projects"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore",
                "InputCore",
                "EditorFramework",
                "EditorStyle",
                "UnrealEd",
                "LevelEditor",
                "InteractiveToolsFramework",
                "EditorInteractiveToolsFramework",
                "LiveCoding",
                "HTTPServer"
            }
        );
    }
}
