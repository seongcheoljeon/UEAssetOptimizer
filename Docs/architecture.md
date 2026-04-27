# Architecture

> **학습자 노트**: 이 문서는 모듈 구조와 데이터 플로우를 보여줍니다. 코드를 순서대로 읽고 싶다면 [`walkthrough.md`](walkthrough.md), 각 결정의 배경을 알고 싶다면 [`decision_log.md`](decision_log.md) 를 참조하세요.

---

## Module layout

```
UEAssetOptimizer (plugin)
│
├── UEAssetOptimizerEditor          ← 유일한 module, editor 전용
│   │
│   ├─ Module lifecycle
│   │   FUEAssetOptimizerEditorModule    IModuleInterface 구현
│   │
│   ├─ UI / Entry point
│   │   FAssetPrepActions                Content Browser 우클릭 메뉴 등록
│   │   ModalParamDialog<T>              USTRUCT 모달 다이얼로그 (Slate)
│   │   BatchOperation<T,F>              filter→dialog→SlowTask→toast 템플릿
│   │
│   ├─ Features
│   │   ULODGenerator                    meshoptimizer 래핑 (LOD 생성)
│   │   UAlphaWrapper                    CGAL::alpha_wrap_3 래핑
│   │
│   └─ Shared helpers
│       MeshAssetOps                     UStaticMesh 쓰기 4종 (LOD/Wrap 공용)
│       CGALIncludes.h                   CGAL/Boost 매크로 격리 (필수 사용)
│       MeshOptIncludes.h                meshoptimizer 단순 wrapper
│
└── ThirdParty (모두 ModuleType.External — UE가 컴파일 안 함)
    ├── MeshOpt           MIT,  vcpkg static lib `meshoptimizer.lib`
    ├── CGAL              GPL,  header-only (`Source/ThirdParty/CGAL/include/`)
    ├── CGALBoost         BSL,  header-only (이름 prefix: UE 엔진의 `Boost` 와 충돌 회피)
    ├── GMP               LGPL, vcpkg import lib `gmp.lib` + DLL `gmp-10.dll`
    └── MPFR              LGPL, vcpkg import lib `mpfr.lib` + DLL `mpfr-6.dll`
```

### 의존성 그래프

```
                UEAssetOptimizerEditor
                          │
        ┌─────────┬───────┴──────┬─────────┐
        │         │              │         │
      UE          CGAL        MeshOpt    (UE Slate /
   modules        │            (lib)      PropertyEditor /
                  ├─ CGALBoost            UnrealEd / etc.)
                  ├─ GMP
                  └─ MPFR
```

`PrivateDependencyModuleNames` (in `UEAssetOptimizerEditor.Build.cs`) 한 곳에 다 명시.

---

## Data flow

### Generate LODs

```
Content Browser 우클릭 (StaticMesh 1+개 선택)
        ↓
FAssetPrepActions::OnGenerateLODsClicked
        ↓
UEAOpt::RunBatchOperation<FLODGenerationParams>(
    SelectedAssets, "Generate LODs", ..., [&](Mesh, Params) {
        return ULODGenerator::GenerateLODs(Mesh, Params);
    })
        ↓ (BatchOperation 내부)
FilterStaticMeshes  →  ShowModalParamDialog  →  FScopedSlowTask 루프  →  NotifySummary
        ↓ (per-mesh 루프 안)
ULODGenerator::GenerateLODs:
   1. Source LOD0 FMeshDescription
        ↓
   2. BuildFlatMeshFromDescription
      (wedge dedup by (pos, normal, uv), output: FFlatMesh)
        ↓
   3. for each TargetRatio:
        SimplifyFlatMesh
        meshopt_simplifyWithAttributes(positions, attrs={N,UV}, weights={1,1,1,0.5,0.5},
                                       target_index_count, target_error, options)
            options &= ~SimplifyLockBorder  (wedge mesh 의 fake border 회피)
        OptimizeVertexOrder
            meshopt_optimizeVertexCache + meshopt_optimizeVertexFetchRemap
        BuildDescriptionFromFlatMesh → 새 FMeshDescription
        ↓
   4. WriteLODsInPlace 또는 CreateLODsAsset (OutputMode 분기)
        ↓ (둘 다 MeshAssetOps 사용)
   for each LOD: CommitLOD(Mesh, i, MD, ScreenSize=sqrt(ratio))
   FinalizeAsset(Mesh, isNew)
        ↓
UStaticMesh::Build() → LOD 슬롯 채워짐
```

### Alpha Wrap

```
Content Browser 우클릭
        ↓
FAssetPrepActions::OnAlphaWrapClicked
        ↓
UEAOpt::RunBatchOperation<FAlphaWrapParams>(
    SelectedAssets, "Alpha Wrap", ..., [&](Mesh, Params) {
        return UAlphaWrapper::CreateAlphaWrap(Mesh, Params) != nullptr;
    })
        ↓ (per-mesh 루프 안)
UAlphaWrapper::CreateAlphaWrap:
   1. BuildTriangleSoupFromStaticMesh
      Source LOD0 FMeshDescription → std::vector<Point_3> + std::vector<array<size_t,3>>
      (wedge dedup 유지, manifoldness 검사 X — soup overload 라 무관)
        ↓
   2. CGAL::Bbox_3 누적 → diagonal 계산 → 절대 alpha/offset
      (Purpose preset 적용: Collision/BakingCage/Cleanup/Custom)
        ↓
   3. CGAL::alpha_wrap_3(points, faces, alpha, offset, wrapped)
      [triangle soup overload — defective input 에 robust, CGAL 공식 권장]
        ↓
   4. (옵션) CGAL::Polygon_mesh_processing::isotropic_remeshing
      bEnableRemeshing=true 일 때만; default false (triangle 수 ↑)
        ↓
   5. BuildMeshDescriptionFromCGAL
      Pass 1: area-weighted smooth normal 계산 ((P1-P0)×(P2-P0), outward)
      Pass 2: vertex instance 생성 + winding swap (V0,V2,V1) for UE CW
              Tangent = arbitrary perpendicular(N), BinormalSign=+1
              UV = (0,0)  [wrap mesh 용도상 UV 안 씀]
        ↓
   6. CreateOrOverwriteSiblingAsset(Source, "_Wrap")
      Material[0] 만 source 에서 상속
      LOD0 = wrapped MeshDescription
      FinalizeAsset
        ↓
새 UStaticMesh `<Source>_Wrap` asset
```

---

## Key Design Decisions (요약)

각 결정의 자세한 배경은 [`decision_log.md`](decision_log.md) 참조.

### 빌드/통합 (`a001d83`)

1. **Editor-only 모듈** — LOD/Wrap 모두 offline 작업. 런타임 의미 X.
2. **meshoptimizer (MIT) for LOD + CGAL (GPL) for Alpha Wrap** — 라이선스 깔끔, irreplaceable 부분만 GPL.
3. **CGALBoost** — UE 엔진의 `Boost` 모듈과 이름 충돌 회피용 prefix.
4. **CGALIncludes.h 격리** — `THIRD_PARTY_INCLUDES_START` + `push/pop_macro`. CGAL을 다른 곳에서 직접 include 금지.
5. **MeshOpt = static lib + DLL 경계 회피** — UBT의 `<MODULE>_API` 매크로 chain 충돌을 우회. vcpkg `x64-windows-static-md`.
6. **GMP/MPFR DLL filename 절대 rename 금지** — `.lib` import 레코드가 정확한 DLL 이름 가짐.

### 알고리즘 (`8be0819` ~ `2cf771c`)

7. **Triangle soup overload** — CGAL 공식 권장. mesh overload 보다 defective input에 robust.
8. **Wedge dedup** — UE FMeshDescription의 vertex_instance를 (pos,normal,uv) 단위로 dedup. meshoptimizer / alpha_wrap 둘 다의 입력 형태.
9. **`bLockBorders=false`** — wedge mesh의 UV seam이 meshopt에게 fake border처럼 보임 → LockBorder 켜면 simplify가 멈춤.
10. **`TargetError=1.0`** (사실상 무제한) — 게임 LOD는 ratio가 primary, error는 보조.
11. **Outward normal = `(P1-P0) × (P2-P0)`** — CGAL halfedge가 CCW 순회하므로 cross product의 right-hand rule이 outward. **Vertex normal은 winding과 무관**.
12. **CW winding swap (V0,V2,V1)** — UE left-handed 좌표계의 front-face 규칙.
13. **Tangent = arbitrary perpendicular** — UV 없는 wrap mesh에서 MikkTSpace 경고 회피용. 셰이딩에 영향 없는 dummy frame.
14. **Material 슬롯 source에서 상속** — nullptr fallback인 dark `DefaultMaterial` 회피.

### UX / 리팩터링 (`0b4588c`, `248b8a7`)

15. **`IStructureDetailsView` modal dialog** — UPROPERTY 메타데이터로 자동 UI. ClampMin/Max, EditCondition, enum 다 자동.
16. **`RunBatchOperation<TParams>` 템플릿 + `MeshAssetOps` 헬퍼** — LOD와 Wrap의 공통 패턴 (filter→dialog→progress→toast / source model + commit) 추출.

---

## Module-by-module 책임 한 줄

| 모듈/파일 | 한 줄 책임 |
|----------|-----------|
| `UEAssetOptimizerEditor.cpp` | 모듈 lifecycle (Startup/Shutdown), `FAssetPrepActions::Register` |
| `AssetPrepActions.cpp` | Content Browser 우클릭 메뉴 등록 + 두 핸들러를 BatchOperation에 위임 |
| `BatchOperation.h/.cpp` | 다중 메쉬 작업 공통 패턴 (filter / modal / SlowTask / toast) 템플릿 |
| `ModalParamDialog.h/.cpp` | USTRUCT를 IStructureDetailsView로 자동 렌더링하는 모달 |
| `LODGenerator.h/.cpp` | meshoptimizer로 LOD 생성. Wedge dedup → simplify → vertex order optimize → 새 MeshDescription |
| `AlphaWrapper.h/.cpp` | CGAL alpha_wrap_3로 watertight wrap 생성. Triangle soup → wrap → outward normal → 새 asset |
| `MeshAssetOps.h/.cpp` | UStaticMesh 에셋 쓰기 4종 (Configure / Commit / CreateOrOverwrite / Finalize) |
| `CGALIncludes.h` | CGAL/Boost 헤더의 매크로 격리 진입점 — CGAL 코드는 무조건 이 파일 통해 include |
| `MeshOptIncludes.h` | meshoptimizer 헤더 단순 wrapper (CGAL과 달리 매크로 충돌 없어 thin) |

---

## 빌드 타임 흐름

```
prebuild_thirdparty.ps1 (Windows PowerShell)
   │
   ├─ vcpkg install gmp:x64-windows mpfr:x64-windows
   │  (동적 .dll + .lib 둘 다 → ThirdParty/{GMP,MPFR}/{lib,bin}/Win64/)
   │
   ├─ vcpkg install meshoptimizer:x64-windows-static-md
   │  (정적 .lib만 → ThirdParty/MeshOpt/lib/Win64/meshoptimizer.lib)
   │
   ├─ Boost 1.85 .zip 다운로드 + 헤더만 추출
   │  (→ ThirdParty/CGALBoost/include/boost/)
   │
   └─ CGAL 6.0.1 .tar.xz 다운로드 + 헤더만 추출
      (→ ThirdParty/CGAL/include/CGAL/)

         ↓ 위 완료 후

UE editor 시작 → UBT 가 .Build.cs 평가 → 모듈 컴파일
   │
   ├─ Editor 모듈은 우리 .cpp 파일들 컴파일 + ThirdParty include 사용
   ├─ ThirdParty External 모듈은 헤더 + .lib 노출만
   └─ 결과: UnrealEditor-UEAssetOptimizerEditor.dll 생성
            (gmp-10.dll, mpfr-6.dll 도 옆에 staging)
```

---

## 다음 단계 (이 코드 마스터 후)

이 프로젝트가 끝난 게 아니라 **현재 학습 가능한 단위로 닫혔다**는 의미. 추가 확장은 본인 페이스로:

- 벤치마크 측정 (1000 instance LOD on/off 프레임타임 비교)
- 데모 영상 / GIF 녹화
- 블로그 포스트: "Integrating CGAL into UE5 — what actually works"
- v2.0 기능들:
  - `UConvexDecomposer` — VHACD 스타일 collision decomposition
  - `UUVUnwrapper` — CGAL Surface_mesh_parameterization (LSCM/ARAP)
  - `USocketGenerator` — 자동 UCX_* collision primitives
- Linux 호스트 지원 (현재 Win64 only)
- GitHub Actions CI (빌드 검증)
- 자동화 테스트 (FAutomationSpec 회귀)
