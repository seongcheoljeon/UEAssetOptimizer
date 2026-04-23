// Copyright (c) 2026 Seongcheol Jeon. Licensed under MIT.
// meshoptimizer is compiled from source in-tree (MIT, tiny, no deps).
// Sources placed into Source/ThirdParty/meshoptimizer/src/ by the prebuild script.
using System.IO;
using UnrealBuildTool;

public class meshoptimizer : ModuleRules
{
	public meshoptimizer(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string IncludePath = Path.Combine(ModuleDirectory, "include");
		if (Directory.Exists(IncludePath))
		{
			PublicSystemIncludePaths.Add(IncludePath);
		}
	}
}
