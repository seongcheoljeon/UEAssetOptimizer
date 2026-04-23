// Copyright (c) 2026 Seongcheol Jeon. Licensed under MIT.
// Boost headers bundled privately for CGAL's exclusive use.
//
// The module is named "CGALBoost" (not "Boost") to avoid a hard conflict with
// the engine module named "Boost" used by UE 5.6's SparseVolumeTexture.
// Project-level modules cannot be referenced from engine modules by UBT's
// hierarchy rule, so the name must differ.
//
// Headers placed into Source/ThirdParty/CGALBoost/include/ by
// Scripts/prebuild_thirdparty.ps1.
using System.IO;
using UnrealBuildTool;

public class CGALBoost : ModuleRules
{
	public CGALBoost(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string IncludePath = Path.Combine(ModuleDirectory, "include");
		if (Directory.Exists(IncludePath))
		{
			PublicSystemIncludePaths.Add(IncludePath);
		}

		// Header-only usage; disable Boost's auto-linking of .lib files.
		PublicDefinitions.AddRange(new string[]
		{
			"BOOST_ALL_NO_LIB=1",
			"BOOST_EXCEPTION_DISABLE=0",
		});
	}
}
