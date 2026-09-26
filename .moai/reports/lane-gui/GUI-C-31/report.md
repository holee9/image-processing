# GUI-C-31 — E2E 스모크 Native 변형 + S-05 semver 강제 (C-30 Gap 1)

- 카드: GUI-C-31 · Refs #136 · 워크트리 `D:/workspace-github/xpe-gui` / `dev/gui`
- 커밋: `4ad90d0` (미푸시) · 선행 C-30(`fb2f991`) 뒤, dev/gui 위에서 바로
- 네이티브: **CI run 34483450020** 아티팩트 (`xpe_common.dll` md5 `c59bb975…`)
- **결과: Mock 5/5 · Native 5/5 · slnx 0/0 · 통합 0/167/1/168 무회귀**

---

## 1. 스테이징을 먼저 검증했다

첫 복사에서 `cp -n`(no-clobber)을 써 **옛 아티팩트가 그대로 남아 있었다.** 타임스탬프가 이전 run(21:39) 그대로인 것을 보고 잡았고, md5 로 교체를 확인했다.

```
run 34483450020 의 xpe_common.dll   c59bb9754de5f0be078e97bdc1e53018
덮어쓰기 전 build/ci-common/bin     a930fca10998a4ee22422b8210063454   ← 옛 run
덮어쓴 뒤                            c59bb9754de5f0be078e97bdc1e53018
```

**"복사했다" 를 "새 바이너리가 있다" 로 읽지 않는다** — 이 레인의 gate #9 와 같은 형태다.

## 2. 구현

| 파일 | 변경 |
|---|---|
| `Fixtures/ApplicationFixture.cs` | `XPE_E2E_BACKEND`(기본 `Mock`)로 `--automation-backend` 결정, 그 밖의 값 거부. Native 면 `XPE_NATIVE_DIR`+`XPE_NATIVE_DIR_EXCLUSIVE=1` 을 앱 프로세스 환경에 전달 |
| `Scenarios/Smoke/SmokeScenarios.cs` | S-05 를 백엔드별로 분기 — Native 는 **semver 만**, Mock 은 C-30 규칙 유지 |
| `gui/ImageProcTest/MainWindow.xaml.cs` | §3 의 배선 결함 수정 |

값 검증은 C-27 규칙과 같다: 인식되지 않는 `XPE_E2E_BACKEND` 는 **거부**한다. 조용히 Mock 으로 떨어지면 무엇을 쟀는지에 대한 보고가 틀리기 때문이다.

## 3. 실측으로 찾은 배선 결함 — `--automation-backend` 가 먹지 않았다

첫 Native 실행이 이렇게 실패했다:

```
Native run: expected a semver from the native backend,
got 'mode=Mock  |  common=v0.0.0-mock  |  display=v0.0.0-mock-display'.
```

**`mode=Mock`** 이 단서다. 인자를 넘겼는데 앱이 Mock 으로 돌았다. 원인: `MainWindow.CreateSettings()` 가 `App.IsAutomationMode`(= raw **와** report 가 모두 있음) 인 경우에만 인자를 반영했다. E2E 픽스처는 **일부러 `--automation-report` 를 주지 않는다**(주면 자기 주행 시나리오가 창을 닫아 모든 단언과 경합한다). 그래서 스위치는 넘어갔지만 아무것도 하지 않았고, 배포 파일 값으로 돌고 있었다.

수정: 인자가 있으면 자동화 모드가 아니어도 백엔드를 선택한다. **설정 격리는 그대로 자동화 모드 전용**이다 — 백엔드는 실행 선택이지 저장된 상태가 아니다.

이것이 이 카드의 실질적 수확이다. C-26~C-30 의 모든 Mock 실행이 통과한 것은 배포 파일이 마침 Mock 이었기 때문이고, **인자가 실제로 작동하는지는 Native 를 요구하기 전까지 아무도 관측하지 못했다.**

## 4. 반증 2회 — 1차는 실험 실패

**1차 (무효).** 카드가 지시한 대로 `XPE_NATIVE_DIR` 를 빈 디렉터리로 주고 Native 를 돌렸다 → **5/5 통과.** 변형이 아무것도 바꾸지 못했다.

원인 실측:

```
grep -rn "XPE_NATIVE_DIR|NativeSearchPolicy|LibraryLocator" gui/ImageProcTest/ --include=*.cs   → 0건
gui/ImageProcTest/Services/Native/XpeDisplayInterop.cs:80  [DllImport("xpe_common.dll", …)]
```

**gui 앱은 `XPE_NATIVE_DIR` 를 읽지 않는다.** 그 탐색 정책(C-16/#129)은 `clients/ImageProcTest` 쪽 로케이터의 것이고, gui 는 평범한 `DllImport` 라 Windows 로더가 **앱 폴더에서** 해석한다. 즉 카드의 전제 중 "Native 면 `XPE_NATIVE_DIR` 로 제어" 는 gui 에 대해 성립하지 않는다. 변수는 계속 전달하되(앞으로 gui 가 로케이터를 채택하면 의미가 생긴다) **오늘은 아무 효과가 없다.**

**2차 (유효).** 실제 지렛대인 앱 폴더의 DLL 을 치웠다:

```
실패!  - 실패: 1, 통과: 4, 전체: 5
S05_RuntimePanel_ShowsBackendVersion:
  Native run: expected a semver from the native backend,
  got 'mode=Native  |  common=v0.0.0-mock  |  display=v0.0.0-mock-display'.
  A mock marker here means the backend silently fell back.
```

**정확히 이 카드가 잡으려던 상황이다** — `mode=Native` 인데 버전은 mock, 즉 조용한 폴백. C-30 의 Mock 전용 규칙으로는 통과했을 케이스다. 원복 후 재확인.

## 5. 실측 (verbatim)

| 실행 | 결과 | 시나리오 합 |
|---|---|---|
| Mock (기본) | `실패 0 / 통과 5 / 건너뜀 0` | 698 ms |
| Native (`XPE_E2E_BACKEND=Native`) | `실패 0 / 통과 5 / 건너뜀 0` | 587 ms |

```
dotnet build clients/ImageProcTest.slnx -c Debug -t:Rebuild
    경고 0개
    오류 0개

dotnet test clients/ImageProcTest.IntegrationTests/ImageProcTest.IntegrationTests.csproj -c Debug
통과!  - 실패:     0, 통과:   167, 건너뜀:     1, 전체:   168
```

Baseline 귀속: 통합 스위트는 C-30 최종 `0/167/1/168` 과 동일(E2E·gui 변경이라 통합 항목을 바꾸지 않는다). 스모크 건수는 5로 불변이며, 이번에 늘어난 것은 **실행 변형**(Mock/Native 2회)이지 케이스 수가 아니다.

## 6. 미검증 (Gaps)

- **Native 실행이 어떤 semver 를 보였는지 라벨 원문을 기록하지 않았다.** 통과 사실만 남겼고, `common=` 값 자체를 로그에 남기지 않았다 — 다음 카드에서 실패 시에만 원문이 보인다.
- **`XPE_NATIVE_DIR` 는 gui 에 대해 무효다**(§4). 전달은 하지만 효과는 없으므로, Native 실행이 무엇을 로드했는지는 **앱 폴더에 무엇을 두었는가**로만 결정된다. 픽스처는 그 폴더를 검사하지 않는다.
- **CI 에서 Native 잡을 돌려 보지 않았다** — 카드가 leader 몫으로 뒀다. CI 는 아티팩트를 앱 폴더로 스테이징해야 하며, `XPE_NATIVE_DIR` 만 설정하면 조용히 Mock 으로 돈다(§4).
- `XPE_E2E_BACKEND` 에 잘못된 값을 준 경우는 코드로만 처리했고 실행으로 확인하지 않았다.
- run 34483450020 은 실행 중(`in_progress`)이었고 빌드 잡 산출물만 받았다 — 그 run 의 최종 결론은 확인하지 않았다.

## 7. 잔여 위험 (Residual risk)

- **Native 실행의 정확성이 앱 폴더 상태에 달려 있다.** 누군가 gui 출력 폴더에 옛 DLL 을 남겨 두면 Native 스모크는 그것을 재고도 통과한다 — S-05 는 "semver 인가" 만 보지 "어느 빌드인가" 는 보지 않는다. 버전 문자열이 빌드를 구분하지 못하는 한 이 위험은 남는다.
- **배선 결함(§3)과 같은 형태가 또 있을 수 있다.** `--automation-*` 인자들은 각각 다른 조건에서 적용되며, 그 조건을 검사하는 테스트는 파서까지만 있다(C-28 가드는 거부 경로만 본다).
- gui 가 `clients` 의 탐색 정책을 쓰지 않는다는 사실은 이번에 처음 문서화됐다. 두 앱이 네이티브를 다르게 찾는다는 것 자체가 진단을 어렵게 한다 — 통합 여부는 별도 결정 사항이다.

## 부록 — 사용한 명령

```bash
export PATH="/c/Program Files/GitHub CLI:$PATH"
gh run download 34483450020 -n xpe-ci-common-binaries -n xpe-ci-post-binaries -D <tmp>
cp -f <tmp>/xpe-ci-common-binaries/*.dll build/ci-common/bin/      # cp -n 은 옛 파일을 남긴다
md5sum build/ci-common/bin/xpe_common.dll                          # 교체 확인

export PATH="/c/Program Files/dotnet:$PATH"
dotnet test clients/ImageProcTest.E2ETests/ImageProcTest.E2ETests.csproj -c Debug                      # Mock
XPE_E2E_BACKEND=Native XPE_NATIVE_DIR="$(pwd)/build/ci-common/bin" dotnet test …                       # Native
dotnet build clients/ImageProcTest.slnx -c Debug -t:Rebuild
dotnet test clients/ImageProcTest.IntegrationTests/ImageProcTest.IntegrationTests.csproj -c Debug
```
