# Build Guide — Windows 11 x64

## Prerequisites

- **Unreal Engine 5.7.4** (source build at `C:\Users\admin\source\repos\UnrealEngine`, confirmed), with editor source available.
- **Visual Studio 2022** (17.8 or newer) with workloads:
  - *Game development with C++*
  - *Desktop development with C++*
  - *MSVC v143 build tools* (toolset 14.38+)
  - *Windows 11 SDK* (10.0.22621+)
- **CMake** 3.25+ on `PATH` (for meshoptimizer; optional if vcpkg builds it).
- **PowerShell 7** (`pwsh`) on `PATH`.
- **vcpkg** on `PATH` — used by the prebuild script to fetch GMP / MPFR binaries.
  ```powershell
  git clone https://github.com/microsoft/vcpkg C:\vcpkg
  C:\vcpkg\bootstrap-vcpkg.bat
  setx PATH "$env:PATH;C:\vcpkg"
  ```
- **Git** for cloning this repo.

## 1. Place the plugin into a UE project

```powershell
cd <YourUEProject>\Plugins
git clone https://github.com/<you>/UEAssetOptimizer.git
```

If the `Plugins` folder does not exist yet, create it next to the `.uproject` file.

## 2. Prebuild third-party libraries

```powershell
cd <YourUEProject>\Plugins\UEAssetOptimizer
pwsh .\Scripts\prebuild_thirdparty.ps1
```

Expected duration: **10–20 min** on first run, ~1 s on subsequent runs (cached).
Pass `-Force` to force refetch/rebuild.

On success you should see these directories populated:

```
Source/ThirdParty/
├── CGAL/include/CGAL/
├── CGALBoost/include/boost/
├── MeshOpt/include/meshoptimizer.h
│   └── lib/Win64/meshoptimizer.lib
├── GMP/include/gmp.h
│   ├── lib/Win64/gmp.lib
│   └── bin/Win64/gmp-10.dll     <-- do NOT rename; .lib imports reference this exact name
└── MPFR/include/mpfr.h
    ├── lib/Win64/mpfr.lib
    └── bin/Win64/mpfr-6.dll     <-- same rule
```

## 3. Generate UE project files

Right-click `<YourUEProject>.uproject` → **Generate Visual Studio project files**.

Or from a shell:
```powershell
& "C:\Users\admin\source\repos\UnrealEngine\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe" `
    -projectfiles `
    -project="<YourUEProject>.uproject" `
    -game -rocket -progress
```

## 4. Build

Open the generated `.sln` in Visual Studio. Build target:
**DevelopmentEditor | Win64**.

First build: **20–40 min** (CGAL/Boost templates are heavy).
Incremental builds: seconds.

Alternatively, just open the `.uproject` in the editor and accept the **"Missing modules"** rebuild prompt.

## 5. Enable the plugin

1. UE Editor → **Edit → Plugins**.
2. Category **Editor Tools** → enable **UEAssetOptimizer**.
3. Restart the editor when prompted.

## 6. Smoke test

1. Import any `.fbx` StaticMesh.
2. Right-click the asset in the Content Browser.
3. Look for the **UEAssetOptimizer** section with entries:
   - *Generate LODs...*
   - *Alpha Wrap...*

Clicking either currently logs a stub message (`LogUEAssetOptimizer`). Full behavior ships in Sprints 2–3.

## Troubleshooting

| Symptom | Cause / Fix |
|---------|-------------|
| `fatal error C1083: Cannot open include file: 'CGAL/...'` | Prebuild script failed or hasn't run. Re-run `pwsh Scripts\prebuild_thirdparty.ps1 -Force`. |
| Macro redefinition `check`, `TEXT`, `PI` | CGAL headers included outside `CGALIncludes.h`. Always use that wrapper. |
| `LNK2019` on `__gmpz_*` or `mpfr_*` | GMP/MPFR libs missing in `Source/ThirdParty/{GMP,MPFR}/lib/Win64/`. Verify `gmp.lib` / `mpfr.lib` exist there. |
| Plugin fails to load at startup after touching CGAL code that uses exact predicates (e.g. `alpha_wrap_3`, Delaunay) | `gmp-10.dll` / `mpfr-6.dll` renamed or missing. Never rename vcpkg DLLs — the `.lib` import records hard-code those exact filenames. |
| Huge compile times | Set `bUseUnity = false` in `UEAssetOptimizerEditor.Build.cs` (already done) and consider enabling `bUseSharedPCHs = true`. |
| Plugin disabled after editor restart | Check `Output Log` for `LogUEAssetOptimizer: startup` — if absent the module failed to load. |
