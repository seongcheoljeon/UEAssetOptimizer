# Decision Log — 시간순 의사결정 기록

이 문서는 코드를 따라 보면서 "왜 이렇게 됐지?" 라고 생각될 때 답을 찾을 수 있도록, **각 결정의 배경**을 시간순으로 기록합니다.

각 항목 형식:
- **상황**: 그 시점에 무엇이 막혔거나 골라야 했나
- **시도**: 어떤 접근을 했나 (실패 포함)
- **결과/배움**: 왜 그 답이었나, 무엇을 배웠나
- **참조**: 관련 커밋, 파일, 문서

> 같이 보면 좋은 문서: 코드 읽는 순서는 [`walkthrough.md`](walkthrough.md), 모듈 구조는 [`architecture.md`](architecture.md).

---

## 프로젝트 시작 (Sprint 1, `a001d83`)

### D1. UE 에디터 플러그인 vs Houdini HDK

- **상황**: 게임 엔진 그래픽스 포트폴리오용 CGAL 활용 프로젝트를 만들고 싶음. 기존 작업(`study_hdk`)은 Houdini HDK 기반인데, 그쪽으로 확장할지 게임 엔진(UE)으로 갈지.
- **시도**: HDK는 본인 친숙도 높지만 게임 엔진 키워드와 거리 있음. UE는 직접 시그널이 강함.
- **결과**: **UE 에디터 플러그인** 으로 결정. "게임 엔진 그래픽스 엔지니어 포지션" 타겟에 직접 매칭. 다만 Houdini Engine for UE를 통한 데모도 가능하니 양립 가능.
- **참조**: `Sprint 1` 초기 plan (`/home/seongcheoljeon/.claude/plans/cgal-elegant-tiger.md` 의 옛 버전)

### D2. LOD는 meshoptimizer (MIT), Alpha Wrap만 CGAL (GPL)

- **상황**: CGAL의 LOD 생성 기능(`Surface_mesh_simplification`)이 GPL이라 상용 사용 시 라이선스 감염. 게임 업계 친화 라이브러리는?
- **시도**:
  - 옵션 1: 모두 CGAL (단일 의존성, 하지만 GPL 전체 적용)
  - 옵션 2: 모두 meshoptimizer (라이선스 깔끔하지만 alpha_wrap 같은 기능 없음)
  - 옵션 3: 혼합 — LOD는 meshoptimizer, Alpha Wrap만 CGAL
- **결과**: **옵션 3** 채택. meshoptimizer는 Unity/Godot 등 게임 엔진의 표준 LOD 라이브러리이므로 친숙도/평판 ↑. CGAL은 alpha_wrap_3 (production-grade 대체 없음) 에만 한정. README 최상단에 "CGAL 부분은 educational/portfolio 한정" 명시.
- **참조**: `README.md` 라이선스 절, `Source/ThirdParty/MeshOpt/MeshOpt.Build.cs`, `Source/ThirdParty/CGAL/CGAL.Build.cs`

### D3. CGALBoost 모듈명 (Boost와 이름 충돌 회피)

- **상황**: 처음엔 ThirdParty 모듈을 단순히 `Boost` 라고 명명. 빌드하니 UBT 에러:
  ```
  Module 'SparseVolumeTexture' (Engine) should not reference module 'Boost' (Project).
  Hierarchy is Project -> Engine Programs -> Engine Plugins -> Engine.
  ```
- **시도**: UE 5.6+ 의 `SparseVolumeTexture` 엔진 모듈이 자체 `Boost` 모듈을 사용하므로 같은 이름이면 모듈 lookup이 깨짐.
- **결과**: 우리 모듈을 `CGALBoost` 로 rename. UBT의 모듈 그래프 충돌 해소. 폴더, Build.cs 클래스, 모든 `PublicDependencyModuleNames` 참조 일괄 수정.
- **배움**: **UE 모듈 이름은 글로벌 유일해야 함** (엔진/프로젝트/플러그인 모두 같은 namespace). 흔한 이름 (Boost, Json, Lib*) 은 prefix 권장.
- **참조**: `Source/ThirdParty/CGALBoost/CGALBoost.Build.cs` (헤더 주석에 이유 기록)

### D4. CGALIncludes.h — UE 매크로 격리 패턴

- **상황**: CGAL 헤더를 그냥 include하면 UE의 `check`, `TEXT`, `PI`, `dynamic_cast` 매크로와 충돌. 컴파일 에러 폭증.
- **시도**: `Boost_PCL_UnrealThirdPartyPlugin` 등 커뮤니티 레포의 패턴 참고:
  ```cpp
  #pragma push_macro("check") #undef check
  ... include CGAL ...
  #pragma pop_macro("check")
  ```
- **결과**: `Source/UEAssetOptimizerEditor/Private/CGALIncludes.h` 단일 격리 헤더. UE의 `THIRD_PARTY_INCLUDES_START/END` 와 결합. **CGAL을 다른 곳에서 직접 include 금지** 를 절대 규칙으로 (헤더 주석에 명시).
- **배움**: UE는 외부 C++ 라이브러리 통합용 매크로 격리 mechanism이 있음 (`THIRD_PARTY_INCLUDES_START`). 추가로 `push/pop_macro` 가 필요한 경우 직접 처리.
- **참조**: `Source/UEAssetOptimizerEditor/Private/CGALIncludes.h`

### D5. Junction Link 워크플로우

- **상황**: 플러그인 코드가 `C:\Users\<user>\source\repos\UEAssetOptimizer\` 에 있지만, UE는 `<UEProject>/Plugins/UEAssetOptimizer/` 를 봄. 매번 복사하면 동기화 지옥.
- **시도**: 단순 복사, symbolic link, junction.
- **결과**: **Windows Junction** (`mklink /J`) 사용. 관리자 권한 불필요, 일반 폴더처럼 작동, 변경 즉시 반영.
- **배움**: UE 플러그인 개발 시 표준적인 dev loop. README "Setup on a new PC" 섹션에 기록.
- **참조**: `README.md` Setup 절

---

## Alpha Wrap 파이프라인 (Sprint 2, `8be0819`)

### D9. Smooth shading용 area-weighted vertex normal 직접 계산

- **상황**: alpha_wrap_3 결과물의 노멀이 zero (없음). UE의 `bRecomputeNormals=true` 로 두면 UE가 자동 계산하지만 결과가 어두운 패치(dark patches) 생김 — 작은/축퇴 트라이앵글이 평균에 잘못 기여.
- **시도**: hard edge 마킹 (flat shading) → 모서리가 보이는 cage 느낌, 사용자 거부. UE recompute → dark patches.
- **결과**: 직접 계산. 각 face의 unnormalized cross product (= 2 * area) 를 incident vertex 들에 누적, 마지막에 정규화 → area-weighted smooth normal. `bRecomputeNormals=false` 로 둬서 우리 값 유지.
- **배움**: UE의 자동 노멀 재계산은 공통 케이스에 좋지만 특수 입력(미세 트라이앵글 많음)엔 직접 계산이 안전. Cross product magnitude가 면적 가중치 역할.
- **참조**: `BuildMeshDescriptionFromCGAL` in `Source/UEAssetOptimizerEditor/Private/AlphaWrapper.cpp` (Pass 1)

### D10. Wrap mesh winding 반전 (UE CW front-face)

- **상황**: 처음엔 CGAL CCW winding 그대로 UE에 전달 → 뷰포트에서 모든 면이 안에서만 보임 ("뒤집힌 느낌"). UE는 left-handed 좌표계라 CW가 front-face.
- **시도**: 그대로 두기 (실패), 모든 노멀 negate (lighting은 맞지만 backface culling 여전히 잘못), winding 자체 반전.
- **결과**: 폴리곤 생성 시 vertex instance를 (V0, V2, V1) 순서로 — 즉 V1, V2 swap. CGAL의 outward 방향이 UE 뷰포트에서 정상적인 front-face가 됨.
- **배움**: Front-face culling 은 winding 으로, lighting은 normal 로 결정. **둘은 별개 개념** — 항상 함께 맞춰야 함.
- **참조**: `BuildMeshDescriptionFromCGAL` Pass 2 in `AlphaWrapper.cpp`

### D11. `bRecomputeNormals/Tangents=false` + 임의 tangent frame

- **상황**: D9에서 노멀은 직접 줬으니 UE recompute 끄면 됨. 하지만 tangent 도 필요 (UE 빌드가 "nearly zero tangents" 경고). UV가 없는 wrap mesh는 MikkTSpace로 tangent 자동 계산 불가.
- **시도**: `bRecomputeTangents=true` + `bUseMikkTSpace=true` → 경고 (zero UV → degenerate tangent). `bRecomputeTangents=false` + zero tangent → 다른 경고.
- **결과**: 직접 임의의 직교 tangent 생성. `Reference = (|N.X|<0.9) ? (1,0,0) : (0,1,0)`, tangent = `cross(N, Reference).Normalize()`. BinormalSign = +1. 셰이딩 영향 없는 더미 frame이지만 경고 제거.
- **배움**: Tangent는 normal map 샘플링용, normal map 없는 mesh에선 zero가 아니어도 임의 직교 frame이면 충분. Wrap mesh의 용도(collision/cage)엔 무관.
- **참조**: `ArbitraryTangent` 람다 in `BuildMeshDescriptionFromCGAL`

### D12. Material 슬롯을 source mesh에서 상속

- **상황**: 새로 생성한 `<Source>_Wrap` asset을 씬에 배치하면 메쉬가 거의 검정. nullptr material slot은 UE의 dark `DefaultMaterial`로 fallback.
- **시도**: 빈 슬롯 (검정), `WorldGridMaterial` 명시 할당 (gray grid 보임), source의 첫 material 상속 (원본과 동일한 룩).
- **결과**: source의 첫 material slot을 그대로 복사. Wrap mesh는 단일 polygon group이므로 첫 슬롯만 의미 있음.
- **배움**: UE에서 nullptr material slot은 DefaultMaterial fallback이며, 이게 의도된 "주의" 표시. 의도된 룩을 원하면 명시 할당 필요.
- **참조**: `CreateWrappedAsset` in `AlphaWrapper.cpp`

---

## LOD Generator (Sprint 3, `09b94ef`)

### D6. meshoptimizer 정적 lib (vcpkg `x64-windows-static-md`) — 4단계 시도

이 결정이 가장 험난했음. 4번 빌드 시도해서 4번 다른 에러 → 마지막에 정답 도달.

**시도 1**: meshoptimizer를 일반 UE 모듈로 빌드 (Type=CPlusPlus, src/*.cpp 자동 컴파일)
- 결과: `MESHOPTIMIZER_API` 매크로가 UBT 자동생성 `MESHOPTIMIZER_API`와 같은 이름이라 충돌 (`int DLLEXPORT redefinition` 에러)

**시도 2**: 모듈 이름을 `MeshOpt` 로 rename → UBT는 `MESHOPT_API` 생성
- 결과: 라이브러리 헤더의 `MESHOPTIMIZER_API` 가 빈 매크로 → 심볼이 DLL에서 export 안 됨 → linker unresolved

**시도 3**: `PublicDefinitions.Add("MESHOPTIMIZER_API=MESHOPT_API")` 로 매크로 chain 연결
- 결과: 라이브러리 src/*.cpp는 UE 헤더(`HAL/Platform.h`)를 include 안 하므로 `DLLEXPORT` 토큰 미정의 → 같은 syntax error 재발

**시도 4 (성공)**: 모듈을 `Type=External` 로 변경하고 vcpkg `x64-windows-static-md` triplet 으로 정적 lib 빌드 → 단순 link
- 결과: DLL 경계 자체가 없으므로 매크로 chain 무관. GMP/MPFR과 동일한 패턴. 끝.

- **배움**:
  - UBT의 `<MODULENAME>_API` 자동 생성은 양날의 칼 — 라이브러리 자체 동명 매크로와 충돌 가능
  - DLL 경계 회피 (static link) 가 매크로 chain 디버깅보다 깔끔
  - vcpkg triplet `x64-windows-static-md` = 정적 lib + 동적 CRT (UE의 /MD 와 매치)
- **참조**: `Source/ThirdParty/MeshOpt/MeshOpt.Build.cs` (헤더 주석에 4단계 이유 기록), `Scripts/prebuild_thirdparty.ps1` Install-MeshOptimizer

### D7. UV seam이 wedge mesh의 fake border가 되는 문제

- **상황**: meshopt simplify가 정확히 같은 결과 (e.g., actual=1356) 를 ratio 0.5/0.25/0.1 모든 케이스에서 반환. 줄어들지 않음.
- **시도**: alpha만 보고 시간 늘려도 동일, error 늘려도 동일. 의심: meshopt가 무엇 때문에 멈추는가.
- **결과**: 진단 — 우리 wedge dedup으로 만든 mesh에서 **UV seam이 별개 wedge로 분리되어 fake border** 처럼 보임. `meshopt_SimplifyLockBorder` 가 이 fake border를 다 잠가서 collapse 불가. **`bLockBorders=false` 가 정답**. (attribute weights가 어차피 seam 정보 보존)
- **배움**: meshopt의 LockBorder는 진짜 open mesh boundary 용. Wedge dedup된 mesh에서는 켜면 안 됨.
- **참조**: `LODGenerator.h` `bLockBorders` 의 코멘트, decision_log 에 더 자세한 진단 기록

### D8. `TargetError = 1.0` (ratio가 주도)

- **상황**: D7 해결 후에도 actual 이 ratio와 다름. error=0.0099 (target 0.01) — error budget 한계 도달이 collapse 멈추는 원인.
- **시도**: TargetError=0.01 (1% relative) 너무 엄격. 게임 LOD 워크플로우에서는 ratio 충실도가 더 중요.
- **결과**: 기본값 1.0 (사실상 무제한). meshopt는 매 collapse마다 가장 cost 낮은 edge를 선택하므로 budget을 풀어줘도 품질은 합리적으로 유지됨. ClampMax도 10.0 으로.
- **배움**: 게임 LOD 표준은 **ratio가 primary, error는 quality cap 보조**. CGAL 권장과 다른 운영 방식. 사용자가 quality floor를 원하면 dial down 가능.
- **참조**: `FLODGenerationParams::TargetError` 코멘트 in `LODGenerator.h`

---

## UX 레이어 (Sprint 4, `0b4588c`)

### D13. Modal dialog: `IStructureDetailsView` in `SWindow`

- **상황**: 우클릭 메뉴가 기본 파라미터로 즉시 실행. 사용자가 ratio/purpose 조정하려면 코드 수정+재빌드 필요. 포트폴리오 부적합.
- **시도**:
  - SCustomDialog (간단하지만 property editor 약함)
  - GetMutableDefault<UObject>() (project settings 스타일이지만 one-off에 무거움)
  - `IStructureDetailsView` + `SWindow` (UE 표준)
- **결과**: 후자. 우리 USTRUCT의 UPROPERTY 메타데이터(`ClampMin/Max`, `EditCondition`, enum, 카테고리)가 자동으로 슬라이더/드롭다운/체크박스로 렌더링. 코드 ~100라인.
- **배움**: UE의 property editor 가 가장 빠른 dialog 솔루션. USTRUCT만 잘 정의하면 UI는 자동.
- **참조**: `Source/UEAssetOptimizerEditor/Private/ModalParamDialog.h/.cpp`

---

## 리팩터링 (`248b8a7`)

### D14. RunBatchOperation 템플릿 + MeshAssetOps 헬퍼 (DRY)

- **상황**: Sprint 4 끝나니 `OnGenerateLODsClicked` 와 `OnAlphaWrapClicked` 흐름이 거의 동일 (filter→dialog→SlowTask→toast). `WriteLODsInPlace`/`CreateLODsAsset`/`CreateWrappedAsset` 도 BuildSettings 세팅 + Commit 패턴 반복.
- **시도**:
  - 함수 inline (DRY 깨짐)
  - 람다 콜백 (시그니처 다양성에 묶임)
  - **템플릿 + 헬퍼 함수** (가장 깔끔)
- **결과**:
  - `BatchOperation.h`: `RunBatchOperation<TParams, FOperation>` 템플릿. 핸들러는 5라인으로 압축.
  - `MeshAssetOps.h/.cpp`: `ConfigureSourceModelDefaults`/`CommitLOD`/`CreateOrOverwriteSiblingAsset`/`FinalizeAsset` 4개 헬퍼.
- **배움**: 템플릿은 "다양한 USTRUCT 파라미터 + 다양한 operation" 처럼 시그니처가 살짝 다른 반복 패턴에 적합. 헬퍼 함수는 "동일 시그니처지만 호출 사이트가 다른" 반복에 적합.
- **참조**: `Source/UEAssetOptimizerEditor/Private/{BatchOperation,MeshAssetOps}.{h,cpp}`

---

## CGAL 공식 권장사항 정렬 (`2cf771c`)

### D15. Triangle soup overload (mesh overload보다 robust)

- **상황**: CGAL 공식 문서 / 예제 ([alpha_wrap_3 examples](https://doc.cgal.org/latest/Alpha_wrap_3/index.html#aw3_examples)) 비교 시, 우리는 `alpha_wrap_3(mesh, ...)` (Surface_mesh overload) 사용. 공식 use case 중 "defective 3D data, triangle soups with intersections and gaps, non-manifold input" 은 **triangle soup overload** 권장.
- **시도**: 메쉬 overload 그대로 두면 우리 `BuildCGALMeshFromStaticMesh` 가 non-manifold 트라이앵글을 silently 건너뜀 → 입력 손실.
- **결과**: `alpha_wrap_3(points, faces, alpha, offset, wrap)` 으로 전환. 입력은 `std::vector<Point_3>` + `std::vector<array<size_t,3>>` (CGAL Example 2 와 동일한 타입). 모든 face가 알고리즘에 입력 — manifold 검사 자체를 안 함.
- **배움**: CGAL 공식 문서를 꼼꼼히 보면 같은 알고리즘에도 입력 형태별 overload가 있고, 본래 use case에 맞는 overload를 쓰는 게 robustness ↑.
- **참조**: `BuildTriangleSoupFromStaticMesh` in `AlphaWrapper.cpp`

### D16. Outward normal 방향 — `(P1-P0) × (P2-P0)` (이전 inward 버그 fix)

- **상황**: Sprint 4 끝나고 wrap mesh에 텍스처 머터리얼 적용 → 라이팅 모드에서 검정. 라이팅 끄면 텍스처 정상.
- **시도**: 머터리얼 슬롯 의심 (D12 점검 — 정상). UV 의심 (zero UV — 텍스처는 한 점 샘플하지만 그래도 보여야 함). **노멀 방향 의심**.
- **결과**: 진단 — `BuildMeshDescriptionFromCGAL` 의 normal 계산이 `(P2-P0) × (P1-P0)` 로 되어 있었음. cross product 성질상 이는 `-(P1-P0) × (P2-P0)` = **inward 방향**. NdotL ≤ 0 → 검정. 수정: edge 순서를 CGAL CCW 그대로 (P0,P1,P2 순회) → outward.
- **배움**:
  - **Vertex normal은 winding 과 무관**. 단지 표면에서 바깥을 가리키는 방향 벡터.
  - 이전 코드 주석에서 "winding 반전했으니 normal도 뒤집어야 한다"는 reasoning은 잘못. winding은 face culling 용 / normal은 lighting 용 — 별개.
  - Lit vs Unlit 모드 비교는 normal 방향 디버깅에 매우 유효.
- **참조**: 수정된 normal 계산 in `BuildMeshDescriptionFromCGAL` Pass 1. 커밋 메시지에 자세한 reasoning 기록.

---

## 메타: 이 프로젝트가 가르쳐주는 것

위 16개 결정을 시간순으로 보면 다음 패턴이 보임:

1. **외부 C++ 라이브러리 + 게임 엔진 통합**은 매크로 / 빌드 시스템 / DLL 경계의 충돌 지점이 많음 (D3, D4, D6)
2. **3D 알고리즘 + 게임 엔진**은 좌표계 / winding / normal / UV 같은 기본기에서 미묘한 버그 발생 (D9~D12, D16)
3. **공식 문서를 꼼꼼히 보면** 더 적절한 API/패턴이 있음을 발견 (D15)
4. **사용자 피드백이 디버깅의 입력** — Sprint 4 끝나고 사용자가 "검게 나와요" 한 마디로 D16 발견
5. **DRY는 사후 리팩터링** — 처음엔 inline으로 빠르게, 패턴이 보이면 템플릿/헬퍼로 추출 (D14)

이 16개 함정/결정 모두 코드의 헤더 주석 / 커밋 메시지 / 이 문서에 흔적이 남아있음. 미래의 본인이 코드 다시 볼 때 같은 함정 안 빠지게 하는 게 이 문서의 목적.
