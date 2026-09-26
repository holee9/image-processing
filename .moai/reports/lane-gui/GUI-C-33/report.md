# GUI-C-33 — 네이티브 해석 출처를 런타임 요약에 노출 + S-05 가 출처를 단언 (#129 #136)

- 카드: GUI-C-33 · Refs #129 #136 · 워크트리 `D:/workspace-github/xpe-gui` / `dev/gui`
- 커밋: `e09effb` (미푸시) · 선행 `git merge origin/main` → `2ef5f30`(CI Native 잡이 `XPE_NATIVE_DIR` 경로로 전환)
- **결과: Mock 5/5 · Native 5/5 · 자동화 Native `NativeSource='bin'` · slnx 0/0 · 통합 0/170/1/171 무회귀**

---

## 1. 무엇을 관측 가능하게 만들었는가

C-32 는 "주입 디렉터리에서 로드된다" 는 **결과**만 보였고, 그 경로가 **리졸버를 탔는지** 로더 기본 동작이었는지 구분하지 못했다. 이 카드가 그 구분점을 만든다.

| 계층 | 값 |
|---|---|
| `GuiNativeLibraryResolver.SourceLabel(dll)` | 리졸버가 기록한 경로의 마지막 디렉터리 이름, 기록 없으면 **`loader`** |
| `BackendRuntimeInfo.NativeSource` | 위 값 (Mock 은 빈 문자열) |
| `RuntimeVersionSummary` | `src=<dir>` 항목 추가 (비어 있으면 항목 자체가 빠진다) |
| `GuiAutomationReport.NativeSource` | 리포트 1필드 |
| S-05 (Native) | `XPE_NATIVE_DIR` 가 지정된 실행이면 `src=` 가 그 디렉터리의 마지막 세그먼트와 **일치** |

**`loader` 가 핵심이다.** 리졸버가 핸들을 주지 않았고 Windows 로더가 앱 폴더에서 해석했다는 뜻이며, 그것이 곧 "리졸버가 호출되지 않았다" 의 관측이다.

`src=` 는 전체 경로가 아니라 마지막 세그먼트만 쓴다 — 상태바 한 줄에 절대경로를 넣으면 잘리고, 판정에 필요한 것은 "**어느 폴더**" 까지다.

## 2. 실행 관측

| 실행 | 조건 | 결과 |
|---|---|---|
| Mock | 앱 폴더에 DLL 있음 | 5/5, `src=` 없음(Mock 은 네이티브를 로드하지 않는다) |
| Native | **앱 폴더 비움 + `XPE_NATIVE_DIR`** (CI 와 같은 조건, `2ef5f30`) | 5/5, `src=bin` 단언 통과 |
| 자동화 Native | 〃 | `Passed=true`, **`NativeSource='bin'`**, `xpe_display 1.0.0` |

## 3. 반증 — 리졸버 설치 줄 제거

`App.OnStartup` 의 `GuiNativeLibraryResolver.Install()` 을 지우고, 로드 자체는 되도록 앱 폴더에 DLL 을 되돌린 뒤 Native 로 실행했다:

```
실패!  - 실패: 1, 통과: 4, 전체: 5
S05_RuntimePanel_ShowsBackendVersion:
  Assert.Contains() Failure: Sub-string not found
  String:    "mode=Native  |  common=xpe_display 1.0.0 "…
  Not found: "src=bin"
```

**`src=bin` 이 사라졌다** — 리졸버가 핸들을 주지 않았고 로더가 앱 폴더에서 해석했기 때문이다. 버전은 여전히 semver 라 C-32 까지의 S-05 규칙으로는 **통과했을** 케이스다. 즉 이 단언이 새로 잡는 것이 정확히 "리졸버가 실제로 호출되는가" 다. 원복 후 재확인.

## 4. 실측 (verbatim)

```
dotnet test clients/ImageProcTest.E2ETests/…                                   (Mock)
통과!  - 실패: 0, 통과: 5, 건너뜀: 0, 전체: 5

XPE_E2E_BACKEND=Native XPE_NATIVE_DIR=<staging> dotnet test …                  (Native, 앱 폴더 비움)
통과!  - 실패: 0, 통과: 5, 건너뜀: 0, 전체: 5

ImageProcTest.exe --automation-backend Native …
exit=0 · Passed=True · BackendMode=Native · NativeSource='bin' · xpe_display 1.0.0

dotnet build clients/ImageProcTest.slnx -c Debug -t:Rebuild
    경고 0개
    오류 0개

dotnet test clients/ImageProcTest.IntegrationTests/… -c Debug
통과!  - 실패:     0, 통과:   170, 건너뜀:     1, 전체:   171
```

Baseline 귀속: 통합 스위트는 C-32 최종 `0/170/1/171` 과 동일 — 이번 변경은 gui 표시·E2E 단언이라 통합 항목을 더하지 않는다(예상값이자 실측값). 스모크도 5건 불변이고 **단언이 강해졌을 뿐**이다.

## 5. 미검증 (Gaps)

- **`src=` 는 디렉터리 이름만 본다.** 스테이징 폴더가 `bin` 이라 `src=bin` 인데, 다른 곳의 `bin` 이어도 같은 값이다 — "어느 폴더" 는 구분하지만 "어느 경로" 는 구분하지 않는다. 전체 경로는 `RuntimeInfo.NativeDllPath` 에 있으나 요약에는 넣지 않았다(§1).
- **`xpe_display.dll` 의 출처는 노출하지 않는다.** `NativeSource` 는 `xpe_common.dll` 하나만 쓴다. 두 DLL 이 다른 폴더에서 올 수 있는데 그 경우를 표현하지 못한다.
- **`SourceLabel` 이 "loader" 를 돌려주는 경로를 정상 실행에서 본 적은 없다** — 반증에서만 관측했다.
- 리포트의 `NativeSource` 는 자동화 모드에서만 채워진다(E2E 스모크는 UI 라벨을 읽는다).
- CI 실행 결과는 아직 없다.

## 6. 잔여 위험 (Residual risk)

- **S-05 가 이제 UI 문자열 두 가지(`mode=`, `src=`)에 묶인다.** 표시 문구를 바꾸면 시나리오가 깨진다 — C-30 에서 시작된 결합이 한 단계 깊어졌다. 대신 그 문구가 진단 가치를 갖게 됐다.
- **`src=` 는 로드 시점의 기록이다.** 리졸버가 한 번 성공한 뒤 DLL 이 교체돼도 라벨은 그대로다 — 프로세스 수명 안에서는 맞지만, 화면 값이 항상 현재 파일을 가리키는 것은 아니다.
- 같은 이름의 폴더가 흔하다(`bin`). CI 가 `XPE_NATIVE_DIR` 를 다른 이름으로 바꾸면 단언은 따라가지만, 사람이 로그만 보고 판단할 때는 모호할 수 있다.

## 부록 — 사용한 명령

```bash
git merge origin/main                                   # → 2ef5f30
export PATH="/c/Program Files/dotnet:$PATH"
XPE_E2E_BACKEND=Native XPE_NATIVE_DIR="$(pwd)/build/ci-common/bin" dotnet test …E2ETests…
XPE_NATIVE_DIR=… XPE_NATIVE_DIR_EXCLUSIVE=1 ImageProcTest.exe --automation-backend Native …
dotnet build clients/ImageProcTest.slnx -c Debug -t:Rebuild
dotnet test clients/ImageProcTest.IntegrationTests/… -c Debug
```
