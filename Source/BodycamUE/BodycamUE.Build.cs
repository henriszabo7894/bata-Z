// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class BodycamUE : ModuleRules
{
	public BodycamUE(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Permet les includes du type "Player/BodycamCharacter.h" depuis n'importe quel sous-dossier.
		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "AIModule", "NavigationSystem", "GameplayTasks", "Slate", "SlateCore" });
	}
}
