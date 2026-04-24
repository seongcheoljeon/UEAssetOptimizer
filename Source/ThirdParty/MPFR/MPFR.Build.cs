// Copyright (c) 2026 Seongcheol Jeon. Licensed under MIT.
// MPFR prebuilt via vcpkg (mpfr:x64-windows). Install layout mirrors vcpkg's:
//   Source/ThirdParty/MPFR/include/mpfr.h
//   Source/ThirdParty/MPFR/lib/Win64/mpfr.lib
//   Source/ThirdParty/MPFR/bin/Win64/mpfr-6.dll
//
// Depends on GMP at runtime.
using System.IO;
using UnrealBuildTool;

public class MPFR : ModuleRules
{
	public MPFR(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string IncludePath = Path.Combine(ModuleDirectory, "include");
		if (Directory.Exists(IncludePath))
		{
			PublicSystemIncludePaths.Add(IncludePath);
		}

		PublicDependencyModuleNames.Add("GMP");

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			string LibPath = Path.Combine(ModuleDirectory, "lib", "Win64");
			string StaticLib = Path.Combine(LibPath, "mpfr.lib");
			if (File.Exists(StaticLib))
			{
				PublicAdditionalLibraries.Add(StaticLib);
			}

			string DllPath = Path.Combine(ModuleDirectory, "bin", "Win64", "mpfr-6.dll");
			if (File.Exists(DllPath))
			{
				RuntimeDependencies.Add("$(BinaryOutputDir)/mpfr-6.dll", DllPath);
			}
		}
	}
}
