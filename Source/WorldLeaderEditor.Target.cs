// Copyright World Leader project. See ROADMAP.md.

using UnrealBuildTool;
using System.Collections.Generic;

public class WorldLeaderEditorTarget : TargetRules
{
	public WorldLeaderEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("WorldLeader");
	}
}
