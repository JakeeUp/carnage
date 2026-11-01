// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class FPS_Movement : ModuleRules
{
	public FPS_Movement(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate",
			"SlateCore",
			"GameplayAbilities",
			"GameplayTags",
			"GameplayTasks",
			"Niagara",
			"DeveloperSettings",
			"PhysicsCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"FPS_Movement",
			"FPS_Movement/Template",       // UE5 first-person template defaults
			"FPS_Movement/Movement",       // custom: Doom/Titanfall movement + momentum
			"FPS_Movement/Camera",         // custom: movement-driven camera juice
			"FPS_Movement/Weapons",        // custom: recoil / sway feel
			"FPS_Movement/Weapons/FX",     // custom: procedural gunfire FX (tracers, flashes, impacts)
			"FPS_Movement/Carnage",        // custom: GAS health/damage layer
			"FPS_Movement/Core",       // the shooter game
			"FPS_Movement/Core/AI",
			"FPS_Movement/Core/UI",
			"FPS_Movement/Core/Weapons",
			"FPS_Movement/Core/Training"   // custom: in-world aim trainer
		});
	}
}
