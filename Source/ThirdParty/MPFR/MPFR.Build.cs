// Copyright (c) 2026 Seongcheol Jeon. Licensed under MIT.
// MPFR depends on GMP. Built alongside GMP by the prebuild script.
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
			string StaticLib = Path.Combine(LibPath, "libmpfr-4.lib");
			if (File.Exists(StaticLib))
			{
				PublicAdditionalLibraries.Add(StaticLib);
			}

			string DllPath = Path.Combine(ModuleDirectory, "bin", "Win64", "libmpfr-4.dll");
			if (File.Exists(DllPath))
			{
				RuntimeDependencies.Add("$(BinaryOutputDir)/libmpfr-4.dll", DllPath);
				PublicDelayLoadDLLs.Add("libmpfr-4.dll");
			}
		}
	}
}
