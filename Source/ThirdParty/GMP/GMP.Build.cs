// Copyright (c) 2026 Seongcheol Jeon. Licensed under MIT.
// GMP static lib built by Scripts/prebuild_thirdparty.ps1 (via vcpkg).
// Output layout:
//   Source/ThirdParty/GMP/include/*.h
//   Source/ThirdParty/GMP/lib/Win64/libgmp-10.lib
//   Source/ThirdParty/GMP/bin/Win64/libgmp-10.dll  (if shared)
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
			string StaticLib = Path.Combine(LibPath, "libgmp-10.lib");
			if (File.Exists(StaticLib))
			{
				PublicAdditionalLibraries.Add(StaticLib);
			}

			string DllPath = Path.Combine(ModuleDirectory, "bin", "Win64", "libgmp-10.dll");
			if (File.Exists(DllPath))
			{
				RuntimeDependencies.Add("$(BinaryOutputDir)/libgmp-10.dll", DllPath);
				PublicDelayLoadDLLs.Add("libgmp-10.dll");
			}
		}
	}
}
