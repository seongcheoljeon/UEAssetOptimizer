// Copyright (c) 2026 Seongcheol Jeon. Licensed under MIT.
// GMP prebuilt via vcpkg (gmp:x64-windows). Install layout mirrors vcpkg's:
//   Source/ThirdParty/GMP/include/gmp.h
//   Source/ThirdParty/GMP/lib/Win64/gmp.lib        (import lib)
//   Source/ThirdParty/GMP/bin/Win64/gmp-10.dll     (shared lib)
//
// IMPORTANT: do NOT rename gmp-10.dll. The .lib's internal import records
// reference the DLL by that exact name; renaming breaks OS-level loading.
using System.IO;
using UnrealBuildTool;

public class GMP : ModuleRules
{
	public GMP(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string IncludePath = Path.Combine(ModuleDirectory, "include");
		if (Directory.Exists(IncludePath))
		{
			PublicSystemIncludePaths.Add(IncludePath);
		}

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			string LibPath = Path.Combine(ModuleDirectory, "lib", "Win64");
			string StaticLib = Path.Combine(LibPath, "gmp.lib");
			if (File.Exists(StaticLib))
			{
				PublicAdditionalLibraries.Add(StaticLib);
			}

			string DllPath = Path.Combine(ModuleDirectory, "bin", "Win64", "gmp-10.dll");
			if (File.Exists(DllPath))
			{
				// Stage next to the plugin DLL so the OS loader finds it via
				// default DLL search path. No DelayLoad: we want normal
				// load-time resolution.
				RuntimeDependencies.Add("$(BinaryOutputDir)/gmp-10.dll", DllPath);
			}
		}
	}
}
