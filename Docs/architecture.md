# Architecture

## Module layout

```
UEAssetOptimizer (plugin)
│
├── UEAssetOptimizerEditor              ← only runtime module (editor-only)
│   ├── FUEAssetOptimizerEditorModule   IModuleInterface lifecycle
│   ├── FAssetPrepActions               Content Browser right-click menu
│   ├── ULODGenerator                   meshoptimizer wrapper
│   └── UAlphaWrapper                   CGAL::alpha_wrap_3 wrapper
│
└── ThirdParty (External ModuleType)
    ├── meshoptimizer      MIT, header + source compiled in-module
    ├── CGAL               GPL, header-only
    ├── CGALBoost          BSL, header-only (renamed from "Boost" to dodge UE module-name conflict)
    ├── GMP                LGPL, prebuilt static lib (vcpkg)
    └── MPFR               LGPL, prebuilt static lib (vcpkg)
```

## Data flow

### Generate LODs
```
Content Browser right-click
        ↓
FAssetPrepActions::OnGenerateLODsClicked
        ↓
ULODGenerator::GenerateLODs(UStaticMesh*, FLODGenerationParams)
        ↓
extract FMeshDescription from LOD0
        ↓
meshopt_simplifyWithAttributes  ×N   (per target ratio)
        ↓
meshopt_optimizeVertexCache / VertexFetch
        ↓
write back as FStaticMeshSourceModel[i].BuildSettings + MeshDescription
        ↓
UStaticMesh::Build() → LOD slots populated
```

### Alpha Wrap
```
Content Browser right-click
        ↓
FAssetPrepActions::OnAlphaWrapClicked
        ↓
UAlphaWrapper::CreateAlphaWrap(UStaticMesh*, FAlphaWrapParams)
        ↓
UE triangles  →  CGAL::Surface_mesh<Point_3>
        ↓
compute bbox diagonal, resolve alpha & offset
        ↓
CGAL::alpha_wrap_3(sm, alpha, offset, wrapped)
        ↓
wrapped → FMeshDescription → new UStaticMesh asset (<Source>_Wrap)
```

## Key design decisions

1. **Editor-only**. Neither LOD simplification nor Alpha Wrap is meaningful at
   runtime — CGAL is not runtime-suitable, and LOD baking is an offline op.
2. **meshoptimizer for LOD, CGAL for Alpha Wrap**. meshoptimizer is MIT
   (license-clean, game-industry standard). CGAL is used only where it is
   irreplaceable (`Alpha_wrap_3` has no production-grade alternative).
3. **Boost stays isolated**. `CGALIncludes.h` is the only header allowed to pull
   in CGAL/Boost. UE core macros (`check`, `TEXT`, `PI`) are saved/restored
   around every CGAL include to avoid cryptic template errors.
4. **ThirdParty binaries not vendored**. vcpkg fetches GMP/MPFR at prebuild
   time. Keeps the repo small and gives us reproducibility.
5. **No runtime DLL dependency if we can help it**. Prefer static builds of
   GMP/MPFR; fall back to DLL + `RuntimeDependencies.Add` if vcpkg only
   provides dynamic.

## Testing strategy

- **Stub smoke tests (Sprint 1)**: Asset Action menu appears; clicking logs.
- **Unit tests (Sprint 2–3)**: bool-returning entry points with empty/null
  inputs, non-manifold inputs, extremely small and extremely large meshes.
- **Integration tests (Sprint 4)**: end-to-end on fixture assets; assert
  triangle count reductions, watertightness (Euler characteristic).
- **Benchmark (Sprint 5)**: 1000-instance scene with LOD on/off, capture
  `stat unit` + `stat rhi` output to JSON.

## Extension ideas (v2.0+)

- `USocketGenerator` — auto-generate UCX_* collision primitives.
- `UMeshAnalyzer` — non-manifold / degenerate tri report, visualization.
- `UConvexDecomposer` — VHACD-style decomposition for articulated physics.
- `UUVUnwrapper` — CGAL Surface_mesh_parameterization integration.
- Linux host support.
