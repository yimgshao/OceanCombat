// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class OceanCombat : ModuleRules
{
	public OceanCombat(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "Water", "WaterAdvanced", "Niagara", "UMG", "Slate", "SlateCore", "AIModule", "GeometryCollectionEngine", "FieldSystemEngine", "ProceduralMeshComponent" });

		PrivateDependencyModuleNames.AddRange(new string[] {  });

		// 编辑器专用(菜单地图烘焙命令 OC.BakeMenuMap):GEditor/资产保存/MeshDescription 构建/资产删除
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[] { "UnrealEd", "MeshDescription", "StaticMeshDescription", "AssetRegistry" });
		}

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
