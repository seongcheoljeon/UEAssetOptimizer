// Copyright (c) 2026 Seongcheol Jeon. Licensed under MIT.
// CGAL is mostly header-only; no libraries to link. Headers placed into
// Source/ThirdParty/CGAL/include/ by Scripts/prebuild_thirdparty.ps1.
using System.IO;
using UnrealBuildTool;

public class CGAL : ModuleRules
{
	public CGAL(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string IncludePath = Path.Combine(ModuleDirectory, "include");
		if (Directory.Exists(IncludePath))
		{
			PublicSystemIncludePaths.Add(IncludePath);
		}

		// CGAL relies on Boost + GMP + MPFR. Downstream modules must also depend
		// on those, but list them here for safety.
		// NOTE: "CGALBoost" (not "Boost") — see CGALBoost.Build.cs for the rename rationale.
		PublicDependencyModuleNames.AddRange(new string[] { "CGALBoost", "GMP", "MPFR" });

		PublicDefinitions.AddRange(new string[]
		{
			"CGAL_HEADER_ONLY=1",
			"CGAL_DO_NOT_USE_BOOST_MP=0",
			"CGAL_USE_GMP=1",
			"CGAL_USE_MPFR=1",
			"BOOST_ALL_NO_LIB=1",
		});

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicDefinitions.Add("NOMINMAX");
		}
	}
}
