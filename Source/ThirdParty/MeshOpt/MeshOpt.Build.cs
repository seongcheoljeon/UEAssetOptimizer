// Copyright (c) 2026 Seongcheol Jeon. Licensed under MIT.
//
// MeshOpt = thin External wrapper around a prebuilt meshoptimizer static lib.
// The library is built via vcpkg (`meshoptimizer:x64-windows-static-md`) by
// Scripts/prebuild_thirdparty.ps1 and dropped into:
//   Source/ThirdParty/MeshOpt/include/meshoptimizer.h
//   Source/ThirdParty/MeshOpt/lib/Win64/meshoptimizer.lib
//
// Why External instead of compiling in-tree:
//   meshoptimizer.h's MESHOPTIMIZER_API macro defaults to empty. Compiling
//   the upstream src/*.cpp as a UE C++ module forces UBT to put symbols
//   inside a DLL, but those symbols never get the right __declspec because
//   the header doesn't reference UE's auto-generated MESHOPT_API. Going
//   prebuilt-static sidesteps the entire DLL boundary, mirroring how
//   GMP/MPFR are integrated.
using System.IO;
using UnrealBuildTool;

public class MeshOpt : ModuleRules
{
	public MeshOpt(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string IncludePath = Path.Combine(ModuleDirectory, "include");
		if (Directory.Exists(IncludePath))
		{
			PublicSystemIncludePaths.Add(IncludePath);
		}

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			string StaticLib = Path.Combine(ModuleDirectory, "lib", "Win64", "meshoptimizer.lib");
			if (File.Exists(StaticLib))
			{
				PublicAdditionalLibraries.Add(StaticLib);
			}
		}
	}
}
