// Copyright World Leader project. See ROADMAP.md.

using UnrealBuildTool;

public class WorldLeader : ModuleRules
{
	public WorldLeader(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Permite includes relativos a la raiz del modulo (Core/..., Campaign/..., Economy/...)
		// sin estructura Public/Private. Necesario con BuildSettingsVersion.V7, que desactiva
		// los include paths heredados (legacy) hacia la raiz del modulo.
		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"Json",
			"JsonUtilities",
			"ProceduralMeshComponent",
			"UMG",
			"CommonUI",
			"DeveloperSettings"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"ImageWrapper",
			"Slate",
			"SlateCore"
		});
	}
}
