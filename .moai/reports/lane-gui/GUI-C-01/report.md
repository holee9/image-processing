# GUI-C-01 — `clients/ImageProcTest` vs `gui/ImageProcTest` 조사 보고서

- 카드: GUI-C-01 (조사 전용, 삭제·이동 없음)
- 워크트리: `D:/workspace-github/xpe-gui` / 브랜치 `dev/gui` / HEAD `cd94288`
- 조사 범위: `clients/**`, `gui/**`, 루트 빌드 파일, `.github/workflows/**`, 문서 참조
- 수정한 파일: 없음 (본 보고서만 생성)

---

## 0. 결론 요약

**두 트리는 중복이 아니다.** 이름만 같고 내용이 전혀 다른 **두 개의 독립 WPF 애플리케이션**이다.
같은 날(2026-04-16) 각각 따로 생성되었고, git 이력상 `gui/` → `clients/` 이동(rename) 커밋은 **존재하지 않는다.**

진짜 문제는 중복이 아니라 **① 정체성 충돌(같은 exe 이름·같은 네임스페이스), ② 문서 드리프트, ③ CI 부재** 세 가지다.

권고안: **통합(merge) 아님 / 삭제 아님 → 둘 다 유지하되 역할·이름·문서를 분리 확정.**

---

## 1. 두 트리의 실제 차이

### 1-1. 동일 이름 5개 파일 — 내용은 전부 다름

`git hash-object`로 blob 해시를 직접 비교한 결과:

| 파일 | 판정 | clients | gui |
|------|------|--------:|----:|
| `App.xaml` | **DIFFERENT** | 8줄 | 7줄 |
| `App.xaml.cs` | **DIFFERENT** | 90줄 | 74줄 |
| `ImageProcTest.csproj` | **DIFFERENT** | 최소 구성 | x64 고정 + WinForms + Content 복사 규칙 |
| `MainWindow.xaml` | **DIFFERENT** | 1,173줄 | 392줄 |
| `MainWindow.xaml.cs` | **DIFFERENT** | 2,348줄 | 368줄 |

> 배정 메시지의 "5파일 동일"은 **파일명 동일**이며 내용 동일이 아니다. 이 전제 정정이 이번 조사의 핵심이다.

### 1-2. 서로 참조하지 않음

`grep -rn 'gui/ImageProcTest' clients` / `grep -rn 'clients/ImageProcTest' gui` → **양방향 0건**.
공유 코드·공유 프로젝트 참조가 전혀 없는 완전 독립 트리다.

### 1-3. 생성 이력 — 이동이 아니라 병렬 생성

```
gui/ImageProcTest/MainWindow.xaml.cs      A  d5432d2  2026-04-16 13:16
clients/ImageProcTest/MainWindow.xaml.cs  A  4dffef4  2026-04-16 22:13
```

`--diff-filter=AD --follow -M`로 확인했으나 **D(삭제) 이벤트 없음, rename 감지 없음.**
즉 `gui/`에서 `clients/`로 옮긴 것이 아니라, 같은 날 약 9시간 간격으로 각각 새로 만들어졌다.

### 1-4. 최종 수정 커밋 — 둘 다 활성

| 트리 | 최종 커밋 | 일시 |
|------|-----------|------|
| `clients/` | `e6f62fc` VVP author IEC 62304 정책 적용 (#93) | 2026-05-09 16:53 |
| `gui/` | `cd94288` 코드 리뷰 3건 critical fix (HEAD) | 2026-05-10 07:30 |

**어느 쪽도 죽은 트리가 아니다.** 24시간 내 양쪽 모두 커밋이 있다.

### 1-5. 역할 구분 (코드 구조로 판별)

| | `clients/ImageProcTest` (88 파일) | `gui/ImageProcTest` (72 파일) |
|---|---|---|
| 성격 | **네이티브 연동·검증 워크벤치** | **운영자용 GUI 셸 (GUI-S0)** |
| 특징 디렉터리 | `Backends/`, `Diagnostics/`, `PInvokeWrappers/` | `Views/`, `Converters/`, `help/`, `fixtures/` |
| 핵심 관심사 | DLL readiness probe, ABI 검증, fixture E2E, metrics | Study queue, VOI/윈도잉 UI, 비교 뷰포트, 오프라인 도움말 |
| 부속 프로젝트 | `ImageProcTest.IntegrationTests` (xUnit, 78 tests) | `ImageProcTest.E2E`, `ImageProcTest.SelfCheck` (ProjectReference로 본체 참조) |
| 런타임 설정 | `app.manifest` | `appsettings.json` + fixture pack |

두 앱의 목적이 서로 다르며, 한쪽이 다른 쪽의 구버전이 아니다.

---

## 2. 루트 CMakeLists / *.slnx 참조

| 항목 | 결과 |
|------|------|
| 루트 `CMakeLists.txt` | `clients` · `gui` · `ImageProcTest` · `csproj` · `dotnet` **전부 0건** (네이티브 전용) |
| `*.slnx` 파일 | 저장소 전체에 **1개**: `clients/ImageProcTest.slnx` |
| 그 slnx 내용 | `ImageProcTest.IntegrationTests/ImageProcTest.IntegrationTests.csproj` **한 줄뿐** |

**중요**: `clients/ImageProcTest/ImageProcTest.csproj`(앱 본체)는 **어떤 솔루션에도 들어있지 않다.**
`gui/` 3개 프로젝트도 솔루션 없음(단, E2E·SelfCheck가 본체를 ProjectReference로 물고 있어 서로 묶여는 있음).

---

## 3. CI가 빌드하는 대상

| 워크플로 | clients | gui | 비고 |
|----------|:-------:|:---:|------|
| `ci.yml` | — | — | CMake 프리셋(`ci-common`, `ci-preprocess`, `coverage`)만. **.NET 빌드 스텝 0개** |
| `windows-common-build.yml` | — | — | `dotnet`/`csproj` 언급 0건 |
| `benchmark-regression.yml` | `clients/**` (트리거 경로 필터) | — | 빌드가 아니라 **실행 조건**일 뿐 |
| `docs-generate.yml` | — | **`gui/**/*.cs`, `gui/**/*.csproj`** (트리거) | `docfx docs/help/docfx/docfx.json` 실행 |
| `docs/help/docfx/docfx.json` | — | **`src: "../../gui"`, `files: ["**/*.csproj"]`** | **저장소에서 유일하게 .NET을 실제로 컴파일하는 경로** |

정리하면:
- **CI가 실제로 빌드하는 C# 트리는 `gui/` 하나뿐**이며, 그것도 API 문서 생성 목적(docfx)이다.
- `clients/`는 트리거 경로로만 등장하고 **빌드·테스트 잡이 없다.** 78개 통합 테스트가 CI에서 돌지 않는다.
- `.moai/project/lane-sessions.md:213`이 이미 "`gui-tests` 잡 없음 → Lane C는 CI 레인 검증을 못 받는다"를 공백으로 기록해 두었다. 본 조사가 이를 재확인한다.

---

## 4. 문서 드리프트 (판정에 필요한 충돌 사실)

| 출처 | 주장 |
|------|------|
| `docs/project/structure.md:34` | ``` `gui/` → `clients/` ``` — **clients가 정본** |
| `README.md:346` | structure v1.4.0에 "`gui/` → `clients/` 경로 반영" |
| `README.md:472` | 빌드 안내가 `clients\ImageProcTest\ImageProcTest.csproj` |
| `codemaps/architecture.md:16` | 아키텍처 다이어그램에 `clients/ImageProcTest/`만 표기 |
| `gui/ImageProcTest/README.md` | 빌드 안내가 `gui\ImageProcTest\ImageProcTest.csproj` |
| `docs/help/docfx/docfx.json` | API 문서 소스는 `gui/` |
| `.moai/project/structure.md:91` | SWU-6.1 테스트 파일이 `gui/ImageProcTest.Tests/QaConstancyTests.cs` — **해당 경로 실존하지 않음** |

문서는 "clients가 정본"이라고 말하는데, 자동화(docfx)와 최신 커밋은 `gui/`를 가리킨다. **문서와 실제가 반대 방향이다.**

`CODEOWNERS`는 이미 `/clients/`·`/gui/` **둘 다** Lane C 소유로 등록되어 있다(배정 메시지의 "최근까지 /clients/만" 상태는 이미 해소됨).

---

## 5. 실제 위험 — 정체성 충돌

두 앱 모두:

- 루트 네임스페이스 `ImageProcTest` (하위 네임스페이스 `ImageProcTest.ViewModels`, `ImageProcTest.Controls` 까지 겹침)
- 출력 어셈블리 `ImageProcTest.exe` (`AssemblyName`·`RootNamespace` 미지정 → 프로젝트명 자동 사용)

따라서 **두 프로젝트를 하나의 솔루션에 넣는 순간 출력 파일명과 타입 이름이 충돌**한다.
현재 솔루션이 사실상 없어서 문제가 드러나지 않고 있을 뿐이며, CI에 `dotnet build` 잡을 추가하는 순간 표면화된다.

---

## 6. 권고안

### 권고: **유지(둘 다) + 역할·이름·문서 분리 확정**

통합(merge)과 삭제를 모두 배제하는 근거:

| 선택지 | 판단 | 근거 |
|--------|------|------|
| 통합(merge) | **반대** | 공유 코드 0줄, 상호 참조 0건, 관심사 분리(네이티브 검증 vs 운영자 UI). 통합은 병합이 아니라 **2,300줄+400줄 전면 재작성**이며 IEC 62304 Class B 추적성을 끊는다 |
| 한쪽 삭제 | **반대** | 24시간 내 양쪽 모두 커밋 존재. `clients/`는 78개 통합 테스트의 대상이고, `gui/`는 docfx가 컴파일하는 유일한 트리. 어느 쪽을 지워도 살아있는 산출물이 사라진다 |
| **유지 + 분리 확정** | **채택** | 실제 상태(두 개의 서로 다른 앱)를 문서·빌드 식별자에 반영하는 것이 최소 변경이자 최소 위험 |

### 후속 조치안 (별도 카드 권고, 이번 카드 범위 밖)

우선순위 순:

1. **[High] 어셈블리 식별자 분리** — 두 `csproj`에 `AssemblyName`/`RootNamespace` 명시 부여
   (예: `XpeIntegrationWorkbench` / `XpeOperatorShell`). 솔루션 통합 시 충돌을 사전 제거.
2. **[High] 문서 정정** — `docs/project/structure.md:34`의 "`gui/` → `clients/`"를 폐기하고, 두 트리의 역할을 각각 기술. `README.md:346`·`codemaps/architecture.md:16`도 동반 정정. IEC 62304 문서 정합성 항목.
3. **[High] 유령 경로 정리** — `.moai/project/structure.md:91`이 가리키는 `gui/ImageProcTest.Tests/QaConstancyTests.cs` 실존 여부 확인 후 경로 수정 또는 항목 삭제.
4. **[Medium] CI 잡 추가** — `dotnet build` + `dotnet test` 잡. `clients/ImageProcTest.IntegrationTests`(78 tests)와 `gui/ImageProcTest.SelfCheck`가 현재 CI에서 전혀 실행되지 않음. `lane-sessions.md:213`이 기록한 공백과 동일 사안.
5. **[Low] 솔루션 정비** — `clients/ImageProcTest.slnx`에 앱 본체 `csproj`가 빠져 있음. 1·2 완료 후 통합 솔루션 도입 검토.

### 미검증 항목 (Gaps)

- 두 앱을 실제로 **빌드/실행해 보지 않았다** (조사 전용 카드, 코드·빌드 산출물 변경 금지 범위). 빌드 성공 여부는 미확인.
- 기능 중복도(예: 두 앱이 같은 네이티브 API를 다르게 호출하는지)는 코드 구조 수준까지만 확인했고 런타임 동작 대조는 하지 않았다.
- `gui/ImageProcTest.Tests/` 경로 부재는 `find`로 확인했으나, 다른 브랜치에 존재할 가능성은 조사하지 않았다.

### 잔여 위험

- 두 트리가 각각 활성인 채로 유지되면 **동일 기능이 양쪽에 따로 구현되는 재중복**이 계속 발생할 수 있다. 권고안 2(문서 역할 정의)가 이 재발을 막는 실질 장치이며, 문서 정정 없이 1번만 하면 이름만 갈라지고 표류는 지속된다.

---

## 부록 — 사용한 검증 명령

```bash
git rev-parse --show-toplevel; git branch --show-current
find clients -type f; find gui -type f
git log -5 --format='%h %ci %s' -- clients/ ; git log -5 --format='%h %ci %s' -- gui/
git log --all --diff-filter=AD --name-status -- gui/ImageProcTest/MainWindow.xaml.cs
git log --all --diff-filter=AD --name-status -- clients/ImageProcTest/MainWindow.xaml.cs
git log --follow -M --name-status -- clients/ImageProcTest/MainWindow.xaml.cs
git hash-object clients/ImageProcTest/<f> ; git hash-object gui/ImageProcTest/<f>   # 5개 파일
grep -nEi 'clients|gui|ImageProcTest|csproj|dotnet' CMakeLists.txt                  # exit 1 (0건)
find . -name '*.slnx' -not -path './third_party/*' ; cat clients/ImageProcTest.slnx
grep -rnE 'clients/|gui/|ImageProcTest' .github/workflows
grep -nE 'clients|gui|csproj|src' docs/help/docfx/docfx.json
grep -rn 'gui/ImageProcTest' clients ; grep -rn 'clients/ImageProcTest' gui         # 양방향 0건
git ls-files clients | wc -l ; git ls-files gui | wc -l                              # 88 / 72
```
