// Copyright (c) 2026 Seongcheol Jeon. Licensed under MIT.
using UnrealBuildTool;

public class UEAssetOptimizerEditor : ModuleRules
{
	public UEAssetOptimizerEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		bUseUnity = false;
		CppStandard = CppStandardVersion.Cpp20;

		// CGAL throws std::exception on invalid input / precondition failures.
		// UE disables exceptions by default; we must opt in for this module so
		// that CGAL unwinding works and try/catch blocks aren't UB.
		bEnableExceptions = true;

		PublicIncludePaths.AddRange(new string[] { });
		PrivateIncludePaths.AddRange(new string[] { });

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore",
			"UnrealEd",
			"AssetTools",
			"ContentBrowser",
			"ContentBrowserData",
			"EditorStyle",
			"EditorSubsystem",
			"EditorScriptingUtilities",
			"Projects",
			"ToolMenus",
			"MeshDescription",
			"StaticMeshDescription",
			"RawMesh",
			"MeshUtilities",
			"MeshReductionInterface",
			"PropertyEditor",
			"InputCore",

			// ThirdParty modules
			"CGAL",
			"CGALBoost",
			"GMP",
			"MPFR",
			"MeshOpt",
		});

		DynamicallyLoadedModuleNames.AddRange(new string[] { });
	}
}
