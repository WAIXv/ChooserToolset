// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ChooserToolset : ModuleRules
{
	public ChooserToolset(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"ToolsetRegistry",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"AssetRegistry",
				"Chooser",
				"CoreUObject",
				"Engine",
				"Json",
				"JsonUtilities",
				"ProxyTable",
				"StructUtils",
				"UnrealEd",
			}
		);
	}
}
