# GUI-C-02 — AssemblyName / RootNamespace 분리 파급 조사

- 카드: GUI-C-02 (H1). **상태: 착수 보류 — lead 판단 대기**
- 워크트리: `D:/workspace-github/xpe-gui` / 브랜치 `dev/gui`
- 변경한 코드: **없음** (`git status --short` 빈 출력). 본 보고서만 생성.
- 선행 카드: GUI-C-01 (PASS, 둘 다 유지 확정)

---

## 0. 요약

지시서의 목표는 "두 앱의 `AssemblyName`/`RootNamespace` 를 분리해 **한 솔루션 공존 시 충돌 제거**" 였다.
조사 결과 두 가지가 드러났다.

1. **개명 파급이 Lane C 범위를 크게 벗어난다** — 바이너리 이름 참조 72개 파일 중 65개가 main 소유(다수가 IEC 62304 규제 문서), 그리고 실행 스크립트 2건이 실제로 깨진다.
2. **목표가 개명 없이도 이미 충족되어 있다** — `AssemblyName` 이 같아도 한 솔루션 공존은 빌드 오류가 아니며, `RootNamespace` 만 바꾸는 것은 타입 충돌을 전혀 해소하지 못한다.

따라서 코드 수정 전에 lead 판단을 요청했다.

---

## 1. 바이너리 이름 참조 파급

`ImageProcTest.exe` / `ImageProcTest.dll` 문자열 참조: **72개 파일**

| 소유 구역 | 파일 수 | Lane C 수정 가능 |
|-----------|-------:|------------------|
| `docs/` | 49 | ✗ (main 소유, IEC 62304 규제 문서 다수) |
| `.moai/` | 16 | ✗ (main 소유) |
| `tools/` | 2 | 불명확 (CODEOWNERS 미등재) |
| `gui/` | 2 | ✓ |
| `tests/`, `SECURITY.md`, `README.md` | 3 | ✗ |

검증 명령:

```bash
grep -rln --exclude-dir=.git --exclude-dir=third_party --exclude-dir=bin --exclude-dir=obj \
  -e 'ImageProcTest\.exe' -e 'ImageProcTest\.dll' . | wc -l          # → 72
grep -rln ... . | sed -E 's|^\./([^/]+).*|\1|' | sort | uniq -c | sort -rn
```

Lane C 는 문서 수정이 금지되어 있으므로, 코드만 개명하면 저장소는 **존재하지 않는 바이너리 이름을 가리키는 main 소유 문서 65개** 상태로 남는다.

## 2. 문서 드리프트가 아니라 실제로 깨지는 참조

| 위치 | 내용 | 영향 |
|------|------|------|
| `tools/e2e/Invoke-ImageProcTestGuiRealE2E.ps1:16` | `$mainExe = ...\gui\ImageProcTest\bin\Debug\net8.0-windows\ImageProcTest.exe` | **gui 개명 시 실 E2E 스크립트 즉시 실패** |
| `tools/ci/Build-DicomModule.ps1:138` | `dotnet clients\...\ImageProcTest.dll --run-preprocess-fixture-e2e` 안내 | clients 개명 시 무효 |
| `tests/test_data/calibration_cases/README.md:45,51` | 동일 명령 2건 | clients 개명 시 무효 |
| `README.md:473-474` | `--probe-native-readiness`, `--run-preprocess-fixture-e2e` 안내 | clients 개명 시 무효 |

## 3. 규제(IEC 62304) 측면

- `.moai/project/structure.md:85` — `clients/ImageProcTest/` → `ImageProcTest.exe` 를 **SWU 매핑 항목**으로 등록 (SWU-5.7 PipelineOrchestrator, SWU-6.1 QaConstancyTest)
- `.moai/project/product.md`, `docs/ai-module/SAD-AI-001` — 같은 이름을 구성항목으로 사용

clients 쪽 exe 개명은 단순 리네임이 아니라 **구성항목 개명 → RTM 추적성 갱신**을 동반한다.

## 4. 전제 재검토 — 개명이 목표 달성에 필요한가

| 검증 항목 | 결과 |
|-----------|------|
| 동일 `AssemblyName` 프로젝트 2개가 한 솔루션에 존재 | **빌드 오류 아님.** 출력 디렉터리가 프로젝트별로 분리됨 |
| 실제 충돌 조건 | (a) 두 산출물을 같은 폴더로 publish, 또는 (b) 한 프로젝트가 두 어셈블리를 동시 참조 |
| 현재 (a) 발생 여부 | 없음 |
| 현재 (b) 발생 여부 | 없음 — `IntegrationTests` 는 clients 소스를 `<Compile Include Link>` 로 4개 링크할 뿐 어셈블리 참조 아님. `E2E`/`SelfCheck` 는 gui 본체만 `ProjectReference` |
| `RootNamespace` 변경의 효과 | **타입 충돌 해소 0.** 소스가 `namespace ImageProcTest` 를 명시 선언하므로 `RootNamespace` 는 신규 파일 기본값·XAML 코드젠에만 영향 |

`x:Class` / `clr-namespace` 조사 결과 두 트리 모두 `ImageProcTest.*` 를 완전 수식으로 사용하며, pack URI(`component/`) 하드코딩은 **0건**이다. 즉 `AssemblyName` 변경 자체는 XAML 리소스 해석을 깨지 않는다.

타입 충돌을 실제로 없애려면 네임스페이스 전면 개명이 필요하고, 규모는 다음과 같다:

| 대상 | clients | gui |
|------|--------:|----:|
| `namespace ImageProcTest*` 선언 `.cs` | 53 | 38 |
| `x:Class` / `clr-namespace` 참조 `.xaml` | 4 | 9 |
| `using ImageProcTest*` 사용처 | 26 | 2 (E2E, SelfCheck) |

합계 약 **132개 파일**. 현재 충돌이 실재하지 않으므로 이번 카드 범위로는 부적절하다고 판단한다.

## 5. lead 에 제시한 3안

| 안 | 내용 | Lane C 단독 완결 |
|----|------|------------------|
| A | 지시서대로 둘 다 개명 | ✗ — main 소유 65파일 동시 수정 필요 |
| **B (권고)** | **gui 만 개명.** clients 는 규제 등록 SWU 구성항목이므로 `ImageProcTest.exe` 유지 | 부분 — 코드+lane 문서는 가능, `tools/*.ps1` 1건과 main 문서 5건은 lead 몫 |
| C | 개명 보류. 실동작 충돌이 없으므로 GUI-C-03(slnx)만 진행 | ✓ — 규제 문서 churn 0 |

B 선택 시 분담:
- Lane C: `gui/ImageProcTest/ImageProcTest.csproj`(AssemblyName), `gui/ImageProcTest/help/quick-start.html`, `gui/ImageProcTest/README.md`
- lead: `tools/e2e/Invoke-ImageProcTestGuiRealE2E.ps1:16`, 그리고 gui 를 exe 이름과 함께 언급하는 main 문서 5건
  (`docs/design/reference/gui-README.md`, `docs/project/XPE-GUI-COMPARE-001`, `docs/project/XPE-GUI-DISP-INT-001`, `.moai/project/structure.md`, `.moai/specs/SPEC-XPE-GUI-IT/research.md`)

`RootNamespace` 는 A/B/C 어느 쪽이든 단독으로는 의미 없음을 함께 보고했다.

---

## 6. M4(CI 잡)용 정보 — lead 요청분

### 6-1. 빌드 대상 csproj (개명 미적용 기준)

```
clients/ImageProcTest/ImageProcTest.csproj                                   WinExe, net8.0-windows, 플랫폼 미지정
clients/ImageProcTest.IntegrationTests/ImageProcTest.IntegrationTests.csproj net8.0, x64 고정, IsTestProject
gui/ImageProcTest/ImageProcTest.csproj                                       WinExe, net8.0-windows, x64, UseWPF+UseWindowsForms
gui/ImageProcTest.E2E/ImageProcTest.E2E.csproj                               gui 본체 ProjectReference
gui/ImageProcTest.SelfCheck/ImageProcTest.SelfCheck.csproj                   gui 본체 ProjectReference
```

개명이 확정되어도 **csproj 파일 경로는 불변**이고 산출물 이름만 바뀐다 → CI 잡의 경로 지정은 재작성 불필요.

### 6-2. 테스트 실행 명령

```bash
dotnet test clients/ImageProcTest.IntegrationTests/ImageProcTest.IntegrationTests.csproj -c Debug
```

- `runs-on: windows-latest` 필수 (net8.0-windows WPF 참조 트리)
- 추가 인자 불필요. `xunit.runner.json` 이 프로젝트에 동봉되어 출력 디렉터리로 복사됨
- **테스트 수 정정**: README 는 "78 tests" 로 기술하나 소스의 `[Fact]`/`[Theory]` 어트리뷰트는 **75건**이다.
  `[Theory]` 의 `InlineData` 확장으로 78 이 될 수 있으나 실행 전 확정 불가 — 이번 카드에서 빌드·실행은 수행하지 않았다.

### 6-3. 네이티브 DLL 의존 — CI 잡 순서에 영향

`ImageProcTest.IntegrationTests.csproj` 의 `CopyXpeDllsForTests` Target (`BeforeTargets="Build"`) 이 다음 4경로를 탐색한다:

```
build/ci-common/bin/Debug/xpe_common.dll
build/ci-common/bin/xpe_common.dll
build/default/bin/Debug/xpe_common.dll
build/default/bin/xpe_common.dll
```

`Copy` 는 `Condition="Exists(...)"` + `ContinueOnError="true"` 이고, 테스트 측 `Fixtures/SkipHelper.cs` 가 DLL 부재 시 조기 return 으로 통과 처리한다.

> **결론**: `common-build` 를 선행시키지 않아도 CI 는 **녹색이 된다.** 그러나 그 경우 네이티브 검증 테스트가 조용히 무의미해진다 — "실행됐지만 아무것도 검증하지 않음". `ci.yml` 의 `common-build` 산출물 경로가 정확히 `build/ci-common` 이므로, dotnet-test 잡을 `needs: [common-build]` 로 걸고 해당 디렉터리를 아티팩트로 인계받는 구성을 권고한다.

---

## 7. 미검증 (Gaps)

- **빌드·테스트를 실행하지 않았다.** `dotnet` 은 `/c/Program Files/dotnet/dotnet.exe` 에 존재하나 `--version` 조회가 실패했고, 착수 보류 상태라 빌드를 시도하지 않았다. 따라서 "현재 두 앱이 빌드된다"는 주장은 하지 않는다.
- 75 vs 78 테스트 수 차이는 실행으로만 확정 가능하며 확정하지 않았다.
- `tools/` 디렉터리의 소유 주체를 CODEOWNERS 에서 확인하지 못했다(미등재). 수정 권한은 lead 위임 대기.
- 개명 후 XAML BAML 리소스 해석이 실제로 정상인지는 빌드로만 확인 가능하며, 미확인이다(정적 분석상 pack URI 0건이라 위험은 낮다고 평가).

## 8. 잔여 위험

- B 안 채택 시 두 트리의 산출물 이름이 비대칭이 된다(`ImageProcTest.exe` vs 신규명). 이는 의도된 것이며 GUI-C-01 결론(역할이 다른 두 앱)과 정합하지만, 문서에 그 의도를 남기지 않으면 다음 세션이 "미완료 개명"으로 오인할 수 있다. lead 의 H2 문서 정정에 이 근거를 함께 기록할 것을 권고한다.
- C 안 채택 시 충돌은 잠복 상태로 남는다. 향후 두 산출물을 같은 폴더로 publish 하거나 두 어셈블리를 동시 참조하는 프로젝트가 생기면 그 시점에 표면화된다.
