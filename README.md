# UEAssetOptimizer

CGAL-powered Unreal Engine 5 **editor plugin** for game asset optimization.

Two features:
1. **Auto LOD Generation** — meshoptimizer-based mesh simplification with UV/feature-edge preservation. Generates multi-level LODs on any `UStaticMesh` with one right-click.
2. **Alpha Wrap** — CGAL `Alpha_wrap_3`-based automatic watertight shrink-wrap. Perfect for generating physics collision meshes or texture-bake cages from dirty scan data.

> Status: **work in progress** (Sprint 1 scaffolding). See [`Docs/architecture.md`](Docs/architecture.md) for the roadmap.

## Why this plugin

Real-time rendering demands optimized assets. This plugin bridges the gap between raw content (scans, sculpts, CAD) and engine-ready geometry, directly inside the Unreal Editor.

- LOD reduces GPU frame time and draw-call cost at distance.
- Alpha Wrap produces watertight meshes from otherwise broken inputs — essential for physics colliders and UV-bake cages.

## Requirements

| Tool | Version | Notes |
|------|---------|-------|
| Unreal Engine | 5.7.4 | Source build verified; Epic Launcher build likely OK |
| Visual Studio | 2022 17.8+ | C++ / Game dev workload, MSVC v143, Windows 11 SDK |
| CMake | 3.25+ | On `PATH`. Install: `winget install Kitware.CMake` |
| PowerShell | 7+ (`pwsh`) | On `PATH`. Install: `winget install Microsoft.PowerShell` |
| Git | any recent | For cloning this repo and vcpkg |
| vcpkg | any recent | Pulls GMP/MPFR Windows binaries. See setup below |
| Platform | Windows 11 x64 | Linux/Mac unsupported in v0.1 |

## Setup on a new PC

One-time, per machine. After this, you can skip to **Build** below.

### 1. Install vcpkg (if not already)

Any directory works — `C:\dev\vcpkg` is a good default.

```powershell
# Clone + bootstrap
New-Item -ItemType Directory -Path C:\dev -Force | Out-Null
cd C:\dev
git clone https://github.com/microsoft/vcpkg
C:\dev\vcpkg\bootstrap-vcpkg.bat

# Add to User PATH (persistent across sessions)
$newPath = [Environment]::GetEnvironmentVariable('PATH','User') + ';C:\dev\vcpkg'
[Environment]::SetEnvironmentVariable('PATH', $newPath, 'User')
```

**Open a new PowerShell window** so the PATH change takes effect, then verify:

```powershell
vcpkg --version
# -> vcpkg package management program version ...
```

If `vcpkg` is still not found in the new window, merge User + Machine PATH into the current session manually:
```powershell
$env:PATH = [Environment]::GetEnvironmentVariable('PATH','Machine') + ';' + [Environment]::GetEnvironmentVariable('PATH','User')
```

### 2. Clone this repo

Two layout options:

**Option A — clone directly into your UE project's Plugins/ folder**
```powershell
cd <YourUEProject>\Plugins
git clone https://github.com/<user>/UEAssetOptimizer.git
```

**Option B — clone elsewhere, symlink via junction (recommended for dev)**
```powershell
# Clone to a stable location
cd C:\Users\<you>\source\repos
git clone https://github.com/<user>/UEAssetOptimizer.git

# Junction-link into each UE test project
cmd /c mklink /J `
    "<YourUEProject>\Plugins\UEAssetOptimizer" `
    "C:\Users\<you>\source\repos\UEAssetOptimizer"
```

Junction means edits in the repo folder are live inside every UE project; no copy/sync needed.

### 3. Run the third-party prebuild script

```powershell
cd <path-to-this-repo>
pwsh .\Scripts\prebuild_thirdparty.ps1
```

What it does (~15–40 min first time, mostly vcpkg building GMP/MPFR):

| Library | Source | License | Where it lands |
|---------|--------|---------|----------------|
| meshoptimizer | GitHub release `.tar.gz` | MIT | `Source/ThirdParty/meshoptimizer/{include,src}` |
| Boost (headers only) | archives.boost.io `.zip` | BSL-1.0 | `Source/ThirdParty/CGALBoost/include/boost` |
| CGAL (headers only) | GitHub release `.tar.xz` | GPL v3 | `Source/ThirdParty/CGAL/include/CGAL` |
| GMP | vcpkg `gmp:x64-windows` | LGPL | `Source/ThirdParty/GMP/{include,lib/Win64,bin/Win64}` |
| MPFR | vcpkg `mpfr:x64-windows` | LGPL | `Source/ThirdParty/MPFR/{include,lib/Win64,bin/Win64}` |

Flags:
- `-Force` — re-fetch & re-install everything, ignoring per-library caches.
- `-BoostVersion '1.85.0'`, `-CGALVersion '6.0.1'`, etc. — override versions if an upstream URL 404s.

### 4. Verify

```powershell
Test-Path Source\ThirdParty\CGAL\include\CGAL\alpha_wrap_3.h        # must be True
Test-Path Source\ThirdParty\CGALBoost\include\boost\version.hpp     # must be True
Test-Path Source\ThirdParty\meshoptimizer\include\meshoptimizer.h   # must be True
Test-Path Source\ThirdParty\GMP\include\gmp.h                       # must be True
Test-Path Source\ThirdParty\MPFR\include\mpfr.h                     # must be True
Test-Path Source\ThirdParty\GMP\lib\Win64\gmp.lib                   # must be True
Test-Path Source\ThirdParty\MPFR\lib\Win64\mpfr.lib                 # must be True
Test-Path Source\ThirdParty\GMP\bin\Win64\gmp-10.dll                # must be True
Test-Path Source\ThirdParty\MPFR\bin\Win64\mpfr-6.dll               # must be True
```

All 7 must print `True`. If any is `False`, re-run with `-Force` and inspect the error.

## Build

Once setup is done (or you're on a machine where it already is):

```powershell
# Open your UE project (.uproject). First launch prompts to rebuild modules.
# Accept; UBT compiles the plugin (20-40 min first time due to CGAL/Boost templates).
```

Or from Visual Studio: open the generated `.sln`, target **DevelopmentEditor | Win64**, Build.

Detailed guide and troubleshooting: [`Docs/build_windows.md`](Docs/build_windows.md)

### Common pitfalls

| Symptom | Fix |
|---------|-----|
| `vcpkg not found on PATH` | PATH not propagated to current shell. Open a new PowerShell window, or manually merge via `$env:PATH = [Environment]::GetEnvironmentVariable('PATH','Machine') + ';' + [Environment]::GetEnvironmentVariable('PATH','User')` |
| `Cannot index into a null array` in prebuild | Old script version. Pull latest; the current script resolves the vcpkg prefix from `(Get-Command vcpkg).Source`. |
| `tar.exe: Archive entry has empty or unreadable filename` on Boost | Windows `tar` can't handle symlinks in `.tar.gz`. Current script uses `.zip` + `Expand-Archive` instead. Pull latest. |
| `Module 'SparseVolumeTexture' should not reference module 'Boost'` | UE 5.6+ has an engine module named `Boost`. Our project module is `CGALBoost` to avoid the collision. Make sure you're on the latest scaffold. |
| `LNK2019` on `__gmpz_*` / `mpfr_*` | `GMP/lib/Win64/gmp.lib` or `MPFR/lib/Win64/mpfr.lib` missing. Re-run prebuild, check vcpkg installed to `C:\<vcpkg-root>\installed\x64-windows`. |
| Plugin fails to load at startup | `gmp-10.dll` / `mpfr-6.dll` not staged next to plugin DLL. Do **not** rename the vcpkg DLLs — their internal import records reference the exact names `gmp-10.dll` and `mpfr-6.dll`. Re-run prebuild. |
| Macro redefinition on `check`, `TEXT`, `PI` | A `.cpp` included CGAL headers outside `CGALIncludes.h`. Always include CGAL only through that wrapper. |

## Usage

1. Enable the plugin: `Edit > Plugins > Editor Tools > UEAssetOptimizer`.
2. In the Content Browser, right-click any `StaticMesh`.
3. Choose **Scripted Actions**:
   - *Generate LODs…* — open LOD parameter dialog, run.
   - *Alpha Wrap…* — open Alpha Wrap parameter dialog, run. A new `<Mesh>_Wrap` asset is created.

## License

Plugin source code: **MIT** (see [`LICENSE`](LICENSE)).

Third-party components retain their own licenses:
- **CGAL** — **GPL v3**. This plugin is intended for **educational and portfolio use**. Commercial use requires a CGAL commercial license from [GeometryFactory](https://www.geometryfactory.com/).
- meshoptimizer — MIT.
- Boost — Boost Software License 1.0.
- GMP / MPFR — LGPL.

## Roadmap

- **v0.1 (Sprint 1–4)**: Build scaffold, Alpha Wrap, LOD, Asset Actions UI.
- **v0.2 (Sprint 5)**: Benchmarks, demo video, documentation.
- **v1.0**: UE 5.7 support, parameter preset library, batch operations.

## Credits

Built as a portfolio project extending [`study_hdk`](https://github.com/) (Houdini HDK study) into the Unreal Engine ecosystem.
