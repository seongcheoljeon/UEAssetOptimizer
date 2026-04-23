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

| Tool | Version |
|------|---------|
| Unreal Engine | 5.7.4 (primary) |
| Visual Studio | 2022 17.8+ with C++ / Game dev workload |
| CMake | 3.25+ |
| PowerShell | 7+ |
| Platform | Windows 11 x64 (Linux/Mac unsupported in v0.1) |

## Build

```powershell
# 1. Clone into your UE project's Plugins/ folder
cd <YourUEProject>/Plugins/
git clone https://github.com/<user>/UEAssetOptimizer.git

# 2. Fetch + build third-party libraries (one-time, ~15 min)
cd UEAssetOptimizer
pwsh Scripts/prebuild_thirdparty.ps1

# 3. Open the UE project. The editor will prompt to rebuild the plugin
#    on first launch. Accept.
```

Detailed guide: [`Docs/build_windows.md`](Docs/build_windows.md)

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
