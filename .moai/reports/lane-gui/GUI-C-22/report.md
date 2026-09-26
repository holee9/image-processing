# GUI-C-22 — 알림 큐 접합 검증: 실제 네이티브 오버플로 → gui 배출 → 표시 (#110)

- 카드: GUI-C-22 · Refs #110 · 워크트리 `D:/workspace-github/xpe-gui` / `dev/gui`
- 선행: `git merge origin/main` → `cd0110f` (QA-A-28 포함, `.claude` 변경 0건)
- 네이티브: **CI run 34477554287** 아티팩트 `xpe-ci-common-binaries` + `xpe-ci-post-binaries` → `build/ci-common/bin/` (2026-09-10 21:39)
- **결과: clients 재빌드 경고 0/오류 0 · gui 재빌드 경고 0/오류 0 · dotnet 실패 0 / 통과 147 / 건너뜀 1 / 전체 148**

---

## 1. 주장 (Claim)

1. 실제 `xpe_common.dll` 의 큐를 실제 `NativeAlertDrain` 으로 배출해 §5.17 을 관측했다 — Info 65건 투입 → 유실 알림 1건(`N=2`, Error) + 페이로드 63건 순서 보존.
2. `xpe_get_pending_alert` 반환 규약 3건을 실행으로 고정했다(빈 큐 / 범위 밖 / 짧은 버퍼).
3. WPF 앱을 **실제로 실행**해 Native 백엔드로 표시 파이프라인이 돌고 배출 코드가 그 경로에 있음을 관측했다 — 다만 **네이티브 알림이 UI 에 뜨는 장면은 관측하지 못했다**(§4·§6, 생산자 부재).
4. 헤더 문서와 구현이 어긋난 지점 2곳을 관측했다(§5).

## 2. 스테이징이 실제 A-28 바이너리임을 먼저 확인

소스가 아니라 **내가 실행할 바이너리**를 확인했다(gate #9).

```
grep -c "alert queue overflow" build/ci-common/bin/xpe_common.dll   → 1
grep -c "xpe_alert_push"       build/ci-common/bin/xpe_common.dll   → 1
ls -l build/ci-common/bin/xpe_common.dll → 2026-09-10 21:39
```

또한 실행 결과에 **건너뜀 0건**이 나온 것이 픽스처가 DLL 로드에 성공했다는 증거다 — `SkipHelper` 가 걸렸다면 4건 전부 Skipped 로 집계된다.

## 3. 실큐 접합 (카드 1항) — `step2-realqueue.log`

```
dotnet test … --filter "FullyQualifiedName~AlertQueueJunctionTests"
통과!  - 실패: 0, 통과: 4, 건너뜀: 0, 전체: 4
```

`RealOverflow_DrainsAs63InfoInOrder_PlusOneErrorLossNotice` 가 관측한 것:

| 항목 | 관측값 |
|---|---|
| `xpe_alert_push` Info 65건 후 `xpe_get_pending_alert_count()` | **64** |
| 배출 결과 중 페이로드(`NATIVE_ALERT`) | **63건**, 전부 `INFO` |
| 페이로드 순서 | `payload alert 3` … `payload alert 65` — 가장 오래된 2건(1,2)이 축출 |
| 유실 알림 | 1건, `ERROR`, 코드 `ALERT_QUEUE_OVERFLOW` |
| 유실 알림 문구(포맷터 통과) | `2 alerts were dropped because the alert queue overflowed.` |
| `xpe_clear_alerts()` 후 count | **0** |

카드가 적은 "첫 오버플로 N=2, 유효 용량 63" 과 일치한다. C-19 가 고정한 표시 문구와 C-21 의 배출 로직이 실제 큐 위에서 맞물린다.

### 반증 — `step3-falsify.log`

유실 알림 문구를 `3 alerts …` 로 바꿨다.

```
실패!  - 실패: 1, 통과: 3, 전체: 4
Expected: "3 alerts were dropped because the alert q"…
Actual:   "2 alerts were dropped because the alert q"…
```

**Actual 이 큐에서 실제로 나온 값**이라는 점이 중요하다 — 이 케이스가 고정 문자열을 자기 자신과 비교하는 공허한 테스트가 아님을 보인다. 원복 후 재확인함.

## 4. 반환 규약 관측 3건 (카드 2항)

| 상황 | 관측 |
|---|---|
| 빈 큐, `index 0` | `INVALID_INPUT` (OK + 빈 문자열이 아니다) |
| `count=1`, `index 5` / `index -1` | 둘 다 `INVALID_INPUT` |
| 메시지 300자, 버퍼 8자 | `BUFFER_TOO_SMALL`. 이어서 `NativeAlertDrain` 이 256 → 8192 재시도로 **전문 300자를 복구** |

세 번째가 C-21 이 가정만 했던 부분이다. 재시도 경로가 실제 DLL 상대로 발화한다.

## 5. 헤더 문서 ↔ 구현 불일치 2건 (관측, 수정하지 않음)

`modules/**` 는 이 레인 소유가 아니라 **보고만** 한다.

| 위치 | 헤더 문서 | 구현 (`xpe_common.cpp:359`) |
|---|---|---|
| `xpe_error.h:113` | 버퍼가 짧으면 "truncated and null-terminated" | `XPE_ERR_BUFFER_TOO_SMALL` 반환, 복사하지 않음 |
| `xpe_error.h:116` | `severity` 는 "May be NULL" | `!severity` 면 `INVALID_INPUT` |

구현 쪽이 더 안전한 동작이고 gui 배출도 구현에 맞춰져 있다. 문서를 구현에 맞추는 편이 맞아 보이지만 판단은 leader/Lane A 몫이다.

## 6. WPF 실행 관측 (카드 3항) — **부분 관측. 네이티브 알림이 뜨는 장면은 보지 못했다**

FlaUI 는 없지만 앱에 자체 자동화 모드(`App.xaml.cs` `--automation-raw/--automation-report`)가 있어 그것으로 **실제 WPF 프로세스를 띄워** 파이프라인 1회를 돌렸다.

```
ImageProcTest.exe --automation-raw fixtures/gui-s0/raw/synthetic_1024x1024.raw \
  --automation-report …/wpf-automation-report.json --automation-width 1024 --automation-height 1024
exit=0
```

`wpf-automation-report.json` (동봉):

| 필드 | 값 | 의미 |
|---|---|---|
| `BackendVersion` | `xpe_display 1.0.0` | **Mock 아님** (`MockXpeBackend` 는 `v0.0.0-mock`) → 실제 DLL 의 `RealXpeBackend` |
| `DisplayPipelineApplied` | `true` | 배출 호출이 들어 있는 `ApplyDisplayPipeline` 이 실행됨 |
| `InitialAlertCount` / `AlertCountAfterLoad` | 1 / 1 | UI 알림 목록에 항목이 표시됨 — 다만 이 1건은 앱 자체의 `REAL_DISPLAY_BACKEND_ACTIVE` |
| `AlertCountAfterClear` | 0 | 지우기 동작 정상 |
| `RuntimeStateAfterShutdown` | `Shutdown` | 정상 종료, 예외 없음 |

**관측한 것**: 실제 DLL 로 표시 파이프라인이 끝까지 돌았고, 그 경로에 들어간 배출 코드가 파이프라인을 깨뜨리지 않았으며, 알림 목록이 UI 에 표시된다.

**관측하지 못한 것**: 네이티브 큐에서 나온 알림이 목록에 뜨는 장면. 이유는 실측으로 확인했다 —

```
grep -rn "xpe_alert_push" modules/*/src/*.cpp
  modules/enhance_basic/src/exposure_index.cpp:116
```

**알림을 밀어 넣는 생산자는 `enhance_basic` 하나뿐이고, gui 표시 파이프라인은 그 모듈을 호출하지 않는다.** 그래서 이 경로에서는 큐가 항상 비어 있고 배출은 조기 반환한다. 앱을 어떻게 조작해도 지금은 네이티브 알림이 뜰 수 없다 — 앱에 시험용 push 코드를 넣는 것은 프로덕션 오염이라 하지 않았다.

### 부수 관측 — `Passed=false` 는 내 변경 탓이 아니다

리포트의 종합 판정이 `false` 였다. 단정하지 않고 실험했다: **배출 호출만 주석 처리하고 같은 자동화를 재실행**(`wpf-nodrain.json`).

```
배출 ON : Passed=False  VoiPresetApplied=False  AlertCountAfterLoad=1
배출 OFF: Passed=False  VoiPresetApplied=False  AlertCountAfterLoad=1
```

판정 술어 30여 항목 중 거짓인 것은 `VoiPresetApplied` 하나뿐이고, 배출 유무와 무관하다. Native 백엔드에서 VOI 프리셋이 적용되지 않는 **기존 사안**이며 이 카드 범위 밖이다(후속 후보). 실험 후 원복했다.

## 7. `xpe_calib_save` 선언 확인 (카드 추가 지시)

A-29 의 3인자 변경이 조용한 실패를 낼 수 있는 **P/Invoke 선언은 없다.** 발견된 2곳은 모두 **export 이름 문자열 목록**이다.

| 위치 | 형태 | 인자 어긋남 위험 |
|---|---|---|
| `clients/ImageProcTest/Diagnostics/XpePreprocessReadinessProbe.cs:24` | 필수 export 이름 목록의 문자열 | 없음 (호출하지 않음) |
| `clients/ImageProcTest.IntegrationTests/PInvoke/XpePreprocessNative.cs:74` | 동일하게 이름 목록 문자열 | 없음 |

`gui/**` 에는 등장하지 않는다. 즉 지금은 영향 없고, 누군가 실제 `DllImport` 를 추가하는 시점에 3인자로 맞추면 된다.

## 8. 실측 (verbatim)

```
dotnet build clients/ImageProcTest.slnx -c Debug -t:Rebuild
    경고 0개
    오류 0개

dotnet build gui/ImageProcTest/ImageProcTest.csproj -c Debug -t:Rebuild
    경고 0개
    오류 0개

dotnet test clients/ImageProcTest.IntegrationTests/ImageProcTest.IntegrationTests.csproj -c Debug
통과!  - 실패:     0, 통과:   147, 건너뜀:     1, 전체:   148
```

Baseline 귀속: 직전 C-21 최종 `0/143/1/144`(`GUI-C-21/step5-final.log`) → 증가 4 = 이번 신규 4건. 건너뜀 1은 C-12 부터의 `-15` 문자열 건으로 변동 없음.

첫 빌드에서 경고 1건(xUnit2031, `Assert.Single` 앞 `Where`)이 나와 `Assert.Single(collection, predicate)` 로 교정한 뒤 0으로 되돌렸다.

## 9. 미검증 (Gaps)

- **네이티브 알림이 UI 에 렌더된 장면**(§6). 생산자가 표시 파이프라인 경로에 없어서 앱 조작만으로는 만들 수 없다.
- **스레드 안전성**. 큐는 뮤텍스로 보호되지만 배출 중 다른 스레드가 push 하는 상황은 만들지 않았다.
- **`xpe_clear_alerts` 와 배출 사이의 경쟁**. 배출 직후 clear 하는 현재 구조에서 그 사이에 도착한 알림은 읽히지 않고 지워진다 — 관측하지 않았고, 단일 스레드 파이프라인에서는 발생하지 않는다.
- **64건을 크게 넘는 투입**(예: 200건)에서 N 이 계속 정확한지. 65건 1회만 관측했다.
- CI 실행 결과는 아직 없다(run 34477554287 은 아티팩트만 사용, dotnet 잡 결과는 미확인).

## 10. 잔여 위험 (Residual risk)

- **배출 지점에 생산자가 없다**(§6). C-21 의 배선은 정확하지만 현재로선 사실상 휴면이다. `enhance_basic` 을 호출하는 경로가 gui 에 생기거나, 배출 지점을 전처리 호출 뒤로도 넓혀야 실효가 생긴다 — leader 결정 사항.
- `xpe_common.dll` 두 아티팩트(common/post)가 같은 이름으로 내려온다. common-build 것을 먼저 복사해 그것이 남았고 둘 다 같은 커밋 산출물이지만, 스테이징 순서에 의존하는 구조라 뒤집히면 조용히 다른 바이너리를 쓰게 된다.
- 자동화 리포트의 `Passed` 는 VOI 프리셋 때문에 Native 모드에서 항상 false 다. 이 값을 회귀 신호로 쓰면 이후 진짜 실패를 가린다.

## 부록 — 사용한 명령

```bash
git fetch origin main && git merge origin/main            # → cd0110f
export PATH="/c/Program Files/GitHub CLI:$PATH"
gh run download 34477554287 -n xpe-ci-common-binaries -D <tmp>
gh run download 34477554287 -n xpe-ci-post-binaries   -D <tmp>
cp <tmp>/*/*.dll build/ci-common/bin/
export PATH="/c/Program Files/dotnet:$PATH"
dotnet test … --filter "FullyQualifiedName~AlertQueueJunctionTests"      # 실큐 + 반증
ImageProcTest.exe --automation-raw … --automation-report …              # WPF 실행 관측 2회
dotnet build clients/ImageProcTest.slnx -c Debug -t:Rebuild
dotnet build gui/ImageProcTest/ImageProcTest.csproj -c Debug -t:Rebuild
dotnet test clients/ImageProcTest.IntegrationTests/ImageProcTest.IntegrationTests.csproj -c Debug
```
