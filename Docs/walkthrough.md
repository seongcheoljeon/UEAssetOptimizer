# Walkthrough — 코드 읽는 순서

이 프로젝트를 처음 보는 학습자(미래의 본인 포함)가 코드를 따라가며 이해할 수 있도록 추천 순서를 제시합니다. 각 단계는 **어떤 파일을 보고, 어떤 패턴을 익히고, 다음으로 무엇을 봐야 하는지**를 설명합니다.

> 같이 보면 좋은 문서: [`decision_log.md`](decision_log.md)는 각 결정의 시간순 배경(왜 이렇게 됐는지)을, 이 문서는 코드 읽는 순서(무엇을 보는지)를 다룹니다.

---

## 사전 준비

읽기 전에 다음 배경이 있으면 수월합니다:

- C++17+ 기본 (템플릿, 람다, `std::vector`, `std::array`)
- UE의 `Build.cs` / 모듈 시스템 개념 ("UE 플러그인이 모듈로 구성된다" 정도)
- CGAL이 계산기하학 라이브러리라는 인식
- meshoptimizer가 메쉬 최적화 라이브러리라는 인식
- Windows + Visual Studio 빌드 환경 (UE 자체 빌드 경험이면 충분)

---

## 추천 읽기 순서

### Stage 1 — 프로젝트 개요 (15분)

**1. `README.md`**
- 무엇을 보는가: 프로젝트의 두 핵심 기능 (자동 LOD, Alpha Wrap), 빌드 절차, 라이선스 구조
- 익히는 패턴: GPL CGAL을 어떻게 다루는지 (educational/portfolio 한정 명시)
- 다음으로 → `Docs/architecture.md`

**2. `Docs/architecture.md`**
- 무엇을 보는가: 에디터 모듈 1개 + ThirdParty 5개 (CGAL/CGALBoost/GMP/MPFR/MeshOpt)의 의존 관계, 데이터 플로우 다이어그램, 핵심 설계 결정
- 익히는 패턴: 모듈을 책임 단위로 분할하는 방식
- 다음으로 → `.uplugin` (UE의 진입점)

**3. `UEAssetOptimizer.uplugin`**
- 무엇을 보는가: UE 플러그인 디스크립터 (JSON) — `Modules` 배열, `EngineVersion`, `Plugins` 의존성
- 익히는 패턴: UE가 플러그인을 인식하는 메타데이터 형식, `Type: "Editor"` (런타임 X)
- 다음으로 → 에디터 모듈 Build.cs

---

### Stage 2 — UE 빌드 시스템 (30분)

**4. `Source/UEAssetOptimizerEditor/UEAssetOptimizerEditor.Build.cs`**
- 무엇을 보는가: 에디터 모듈의 의존성 선언 — `Slate`, `PropertyEditor`, `MeshDescription`, 그리고 우리의 ThirdParty 모듈들
- 익히는 패턴:
  - `PublicDependencyModuleNames` vs `PrivateDependencyModuleNames`
  - `bEnableExceptions = true` (CGAL이 던지는 std::exception을 잡기 위함)
  - `bUseUnity = false` (CGAL/Boost 템플릿 컴파일 시간 폭증 방지)
- 다음으로 → 모듈 lifecycle 진입점

**5. `Source/UEAssetOptimizerEditor/Private/UEAssetOptimizerEditor.cpp`**
- 무엇을 보는가: `IModuleInterface::StartupModule/ShutdownModule` 구현, `FAssetPrepActions::Register/Unregister` 호출
- 익히는 패턴: UE 모듈의 lifecycle 진입점, `IMPLEMENT_MODULE` 매크로 의미
- 다음으로 → 우클릭 메뉴 등록 코드

---

### Stage 3 — UI 흐름 (45분)

**6. `Source/UEAssetOptimizerEditor/Private/AssetPrepActions.cpp`**
- 무엇을 보는가:
  - `MakeMeshExtender` — Content Browser 우클릭 메뉴에 진입점 등록 (`FExtender`, `FMenuExtensionDelegate`)
  - `OnGenerateLODsClicked` / `OnAlphaWrapClicked` — 짧은 핸들러 (각 ~10라인)
- 익히는 패턴:
  - UE의 `FContentBrowserModule::GetAllAssetViewContextMenuExtenders()` 등록 흐름
  - 핸들러를 `RunBatchOperation` 템플릿에 위임 (DRY)
- 다음으로 → 그 템플릿 본체

**7. `Source/UEAssetOptimizerEditor/Private/BatchOperation.h/.cpp`**
- 무엇을 보는가: `RunBatchOperation<TParams>` 템플릿 — 다중 메쉬 작업의 공통 패턴
  - filter (사용 가능한 mesh만 추림)
  - modal dialog (USTRUCT 파라미터 편집)
  - `FScopedSlowTask` 진행 다이얼로그 (cancel 가능)
  - `FSlateNotificationManager` 토스트로 결과 요약
- 익히는 패턴: UE 표준 패턴 4개를 한 함수에 조립, 템플릿으로 LOD/Wrap 양쪽 재사용
- 다음으로 → 모달 다이얼로그 헬퍼

**8. `Source/UEAssetOptimizerEditor/Private/ModalParamDialog.h/.cpp`**
- 무엇을 보는가: `ShowModalParamDialog<TStruct>` — UPROPERTY 메타데이터로부터 자동 UI 렌더링
  - `IStructureDetailsView` + `FStructOnScope` (구조체 메모리를 직접 mutate)
  - `SWindow` + `GEditor->EditorAddModalWindow`
- 익히는 패턴: USTRUCT의 UPROPERTY 메타(`ClampMin`, `EditCondition`, `UMETA`)가 어떻게 자동으로 슬라이더/드롭다운/체크박스가 되는지

---

### Stage 4 — Alpha Wrap 깊게 보기 (60분)

**9. `Source/UEAssetOptimizerEditor/Public/AlphaWrapper.h`**
- 무엇을 보는가:
  - `EAlphaWrapPurpose` UENUM (Collision/BakingCage/Cleanup/Custom)
  - `FAlphaWrapParams` USTRUCT (RelativeAlpha/Offset, EditCondition으로 Custom일 때만 활성화)
  - `UAlphaWrapper::CreateAlphaWrap` 정적 함수 시그니처
- 익히는 패턴: enum → 자동 드롭다운, EditCondition으로 동적 활성화
- 다음으로 → CGAL 매크로 격리 헤더

**10. `Source/UEAssetOptimizerEditor/Private/CGALIncludes.h`**
- 무엇을 보는가:
  - `THIRD_PARTY_INCLUDES_START/END` 사이에 `#pragma push_macro/pop_macro`
  - UE의 `check`, `TEXT`, `PI`, `dynamic_cast` 매크로 격리
  - MSVC warning suppression (`C4701`, `C4702` 등)
  - 프로젝트 전역 `using` 별칭 (`CGALKernel`, `CGALPoint3`, `CGALSurfaceMesh`)
- 익히는 패턴: UE 매크로와 외부 C++ 라이브러리 충돌 해결 — **이 헤더 외부에서 CGAL include 금지**가 절대 규칙
- 다음으로 → AlphaWrapper 본체

**11. `Source/UEAssetOptimizerEditor/Private/AlphaWrapper.cpp`** (가장 중요한 파일 중 하나)
- 무엇을 보는가:
  - `BuildTriangleSoupFromStaticMesh` — UE FMeshDescription → `std::vector<Point_3>` + `std::vector<array<size_t,3>>`
  - `CreateAlphaWrap` — 전체 파이프라인 (UE → CGAL → alpha_wrap_3 → CGAL → FMeshDescription → 새 asset)
  - `BuildMeshDescriptionFromCGAL` — CGAL Surface_mesh → FMeshDescription, area-weighted smooth normal 계산
- 익히는 패턴:
  - 메쉬 데이터 모델 변환 (UE wedge ↔ CGAL position)
  - Triangle soup overload (CGAL 공식 권장, mesh overload보다 robust)
  - bbox 대각선 기반 alpha/offset 자동 계산
  - **Outward normal 계산**: CCW edge cross product = 표면 바깥 방향 (left-handed UE 좌표계와 무관)
  - **Winding swap**: CGAL CCW → UE CW front-face (V0,V2,V1)
  - Tangent frame 생성 (UV 없을 때 노멀에 직교하는 임의 벡터)

---

### Stage 5 — LOD Generator (45분)

**12. `Source/UEAssetOptimizerEditor/Public/LODGenerator.h`**
- 무엇을 보는가: `FLODGenerationParams` (TargetRatios 배열, ELODOutputMode), `ULODGenerator::GenerateLODs`
- 익히는 패턴: ratio 배열로 다단계 LOD 컨트롤, OutputMode로 in-place vs 새 asset 분기

**13. `Source/UEAssetOptimizerEditor/Private/MeshOptIncludes.h`**
- 무엇을 보는가: 단순한 thin wrapper — `#include "meshoptimizer.h"` 한 줄
- 익히는 패턴: **모든 외부 라이브러리가 매크로 격리 필요한 건 아니다** — meshoptimizer는 UE 매크로와 안 부딪힘 (CGAL과 대비)

**14. `Source/UEAssetOptimizerEditor/Private/LODGenerator.cpp`**
- 무엇을 보는가:
  - `FFlatMesh` 구조체 + `FWedgeKey` (pos+normal+uv 튜플) — wedge dedup
  - `BuildFlatMeshFromDescription` — FMeshDescription → 평탄한 vertex/index 배열
  - `SimplifyFlatMesh` — `meshopt_simplifyWithAttributes` 호출 (attribute weights로 normal/UV 보존)
  - `OptimizeVertexOrder` — `meshopt_optimizeVertexCache` + `meshopt_optimizeVertexFetchRemap` (GPU 캐시 최적화)
  - `BuildDescriptionFromFlatMesh` — 평탄 배열 → FMeshDescription 역변환
  - `WriteLODsInPlace` / `CreateLODsAsset` — OutputMode 분기
  - `GenerateLODs` — 본체 (ratio 별 simplify → 새 LOD 슬롯)
- 익히는 패턴:
  - **Wedge dedup**: UE FMeshDescription의 vertex_instance(=wedge)를 (pos,normal,uv)로 dedup해서 meshoptimizer 입력 형태로
  - meshoptimizer의 attribute weight (normal=1, UV=0.5) 의미
  - `bLockBorders=false` 의 이유: wedge dedup이 UV seam을 fake border로 만듦
  - `TargetError=1.0` 의 이유: ratio가 주도, error는 느슨하게
  - `ScreenSize = sqrt(triangle_ratio)` (pixel coverage 근사)

---

### Stage 6 — 공용 헬퍼 (15분)

**15. `Source/UEAssetOptimizerEditor/Private/MeshAssetOps.h/.cpp`**
- 무엇을 보는가: 4개 헬퍼 — `ConfigureSourceModelDefaults`, `CommitLOD`, `CreateOrOverwriteSiblingAsset`, `FinalizeAsset`
- 익히는 패턴: LOD와 AlphaWrap 둘 다에서 쓰이던 UStaticMesh 쓰기 패턴 (BuildSettings + CreateMeshDescription + CommitMeshDescription + Build + AssetRegistry)을 4개 함수로 추출 (DRY)

---

### Stage 7 — ThirdParty 통합 (60분, 가장 어려운 구간)

**16. `Source/ThirdParty/CGAL/CGAL.Build.cs`**
- 무엇을 보는가: `Type = ModuleType.External` (UE가 컴파일 안 함, 헤더만 노출), `PublicDependencyModuleNames` 에 `CGALBoost/GMP/MPFR`
- 익히는 패턴: **External 모듈 = 헤더/라이브러리 정의만 노출, 코드 컴파일 안 함**

**17. `Source/ThirdParty/CGALBoost/CGALBoost.Build.cs`**
- 무엇을 보는가: header-only Boost를 "CGALBoost"라는 이름으로 패키징
- 익히는 패턴: **UE 엔진의 SparseVolumeTexture 모듈이 자체 Boost를 사용하므로**, 같은 이름이면 모듈 등록 충돌 → 우리는 prefix를 붙여서 회피

**18. `Source/ThirdParty/GMP/GMP.Build.cs` + `MPFR/MPFR.Build.cs`**
- 무엇을 보는가: External 모듈, `PublicAdditionalLibraries.Add(...lib)` + `RuntimeDependencies.Add(...dll)` (vcpkg가 dynamic build)
- 익히는 패턴: **.lib import 레코드는 정확한 DLL 이름을 가리킴** → DLL 파일명을 rename하면 로더가 못 찾음 (gmp-10.dll 그대로 유지)

**19. `Source/ThirdParty/MeshOpt/MeshOpt.Build.cs`**
- 무엇을 보는가: External 모듈, vcpkg `x64-windows-static-md` triplet으로 정적 lib 받아와서 단순 link
- 익히는 패턴: **DLL 경계 회피 = static lib 사용**. UE의 MESHOPT_API 매크로 chain을 우회. (이전엔 모듈을 `meshoptimizer`로 만들었다가 UBT 자동 생성 매크로가 라이브러리의 `MESHOPTIMIZER_API` 와 충돌해서 빌드 실패 — decision_log D6 참조)

---

### Stage 8 — 외부 라이브러리 자동 설치 (30분)

**20. `Scripts/prebuild_thirdparty.ps1`**
- 무엇을 보는가:
  - vcpkg로 GMP/MPFR/meshoptimizer 설치 (각각 다른 triplet)
  - CGAL/Boost는 GitHub release archive 다운로드 후 헤더만 추출 (Boost는 .zip 사용 — Windows tar의 symlink 문제 회피)
  - 파일을 `Source/ThirdParty/<Module>/<include|lib|bin>/` 으로 배치
- 익히는 패턴: Windows에서 외부 C++ 라이브러리 셋업 자동화, vcpkg triplet 차이 (`x64-windows` 동적 vs `x64-windows-static-md` 정적+/MD)

**21. `Docs/build_windows.md`**
- 무엇을 보는가: 빌드 가이드, "Common pitfalls" 표 — 우리가 실제로 겪은 이슈들
- 익히는 패턴: 미래의 본인이 같은 함정에 안 빠지게 해주는 trouble-shooting 자료

---

### Stage 9 — 의사결정 기록 (필수)

**22. `Docs/decision_log.md`**
- 무엇을 보는가: 16개 주요 결정의 시간순 — 각 결정의 상황 / 시도 / 결과 / 배움
- 익히는 패턴: **이 코드는 처음부터 이렇게 나오지 않았다** — 시행착오를 거친 결과. 코드만 보면 단정해 보이지만 그 뒤엔 4단계 빌드 실패, 노멀 방향 버그, UV seam 함정 같은 흔적이 있음

---

## 학습 목표 체크리스트

위를 모두 따라 읽고 나면 다음을 설명할 수 있어야 합니다:

**UE 빌드 시스템**
- [ ] `.uplugin`과 `Build.cs`의 관계
- [ ] `PublicDependencyModuleNames` vs `PrivateDependencyModuleNames`
- [ ] `Type = ModuleType.External` 의 의미
- [ ] UE 자동 매크로 `<MODULE>_API` 가 어떻게 생성되고 어떻게 충돌하는가

**외부 라이브러리 통합**
- [ ] CGAL의 `MESHOPTIMIZER_API`처럼 라이브러리 자체 API 매크로가 UE의 자동 매크로와 어떻게 충돌하는가
- [ ] DLL 경계 vs Static link의 trade-off
- [ ] `THIRD_PARTY_INCLUDES_START` + `push_macro/pop_macro`로 매크로 격리하는 패턴
- [ ] vcpkg triplet `x64-windows` (동적) vs `x64-windows-static-md` (정적+MD CRT)

**UE 메쉬 데이터 모델**
- [ ] FMeshDescription의 `Vertices` (위치) vs `VertexInstances` (wedge: 위치+normal+uv) vs `Triangles`
- [ ] Wedge dedup이 무엇이고 왜 meshoptimizer 입력에 필요한가
- [ ] FStaticMeshSourceModel의 BuildSettings (`bRecomputeNormals`, `bUseMikkTSpace`)

**기하 알고리즘**
- [ ] CGAL alpha_wrap_3의 alpha (feature size) / offset (shell thickness) 의미
- [ ] CGAL alpha_wrap_3의 4가지 공식 use case (collision/path planning/simulation/repair)
- [ ] Triangle soup overload vs mesh overload의 차이 (defective input robustness)
- [ ] meshoptimizer simplify의 attribute weight 의미
- [ ] 좌표계: UE left-handed CW front-face vs CGAL CCW outward
- [ ] Area-weighted smooth normal 계산법

**UI 패턴**
- [ ] UPROPERTY 메타데이터 → 자동 UI 생성
- [ ] FScopedSlowTask + FSlateNotificationManager 패턴
- [ ] Content Browser 우클릭 메뉴 등록

---

## 다음 학습 단계 (선택)

이 코드를 마스터한 후 더 깊이 파고 싶다면:

- **CGAL Surface_mesh_parameterization** — 진짜 UV unwrap (alpha_wrap에는 의도적으로 안 넣음)
- **CGAL Convex_decomposition** — 비볼록 메쉬를 볼록 조각들로 분해 (게임 물리 collider)
- **meshoptimizer simplifySloppy** — 더 공격적이지만 빠른 LOD 알고리즘
- **UE Niagara** — 우리 wrap 메쉬를 GPU 파티클 충돌 형상으로
- **UE 자동화 테스트** (`FAutomationSpec`) — 메쉬 변환 회귀 테스트
