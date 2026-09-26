# GUI-C-24 — 네이티브 알림 배출을 모든 네이티브 호출의 공통 후처리로 (#134)

- 카드: GUI-C-24 · Refs #134 · 워크트리 `D:/workspace-github/xpe-gui` / `dev/gui`
- 선행: `git merge origin/main` → `dd9cc90` (C-23 병합본, `.claude` 변경 0건)
- **결과: gui 재빌드 0/0 · clients 재빌드 0/0 · dotnet 실패 0 / 통과 150 / 건너뜀 1 / 전체 151** (148 → +3, 카드 예측과 일치)

---

## 1. 네이티브 호출 지점 전수 (카드 1항)

`grep -n "XpeCommonNative\.\|XpeDisplayNative\.\|NativeLibrary" gui/ImageProcTest/Services/RealXpeBackend.cs`

| 줄(변경 전) | 호출 | 속한 공개 동작 | 처리 |
|---|---|---|---|
| :86 | `XpeDisplayNative.GetVersion()` | `GetDisplayVersion()` | **래퍼로 감쌈** |
| :115 | `xpe_alloc_image` | `ApplyDisplayPipeline` | 〃 (동작 단위 1회) |
| :141 | `xpe_apply_modality_lut` | 〃 | 〃 |
| :151 | `xpe_apply_voi_lut` | 〃 | 〃 |
| :158 | `xpe_gsdf_calibrate` | 〃 | 〃 |
| :162 | `xpe_apply_presentation_lut` | 〃 | 〃 |
| :189 | `xpe_free_image` (finally) | 〃 | 〃 |
| :198 | `xpe_voi_preset_create` | `CreateVoiPreset()` | **래퍼로 감쌈** |
| :211 / :232 / :238 | `xpe_get_pending_alert_count` / `xpe_clear_alerts` / `xpe_get_pending_alert` | `DrainNativeAlerts()` 자신 | **제외 — 감싸면 재귀** |
| :377 / :386 / :396 | `NativeLibrary.TryLoad` / `TryGetExport` / `Free` | `CanUseNative()` (static) | **제외 — 인스턴스 생성 전에 도는 정적 프로브라 `_alerts` 가 없다** |

**감싸는 단위는 개별 P/Invoke 가 아니라 공개 동작이다.** 파이프라인의 6개 호출을 각각 감싸면 한 번의 파이프라인 실행에 배출이 6회 돈다 — 카드의 "호출 1회당 배출 1회" 와 어긋난다.

## 2. 구현

| 파일 | 변경 |
|---|---|
| `Services/NativeAlertDrain.cs` | `InvokeWithDrain<T>(Func<T>, Action)` + void 오버로드 — **순수 함수**, `try/finally` |
| `Services/RealXpeBackend.cs` | `InvokeNative<T>` 가 위를 `DrainNativeAlerts` 와 묶는다. `GetDisplayVersion` / `ApplyDisplayPipeline`(→ `ApplyDisplayPipelineCore` 로 분리) / `CreateVoiPreset` 이 이 경로를 탄다 |
| 〃 | 파이프라인 안에 있던 `DrainNativeAlerts()` 직접 호출 **제거** — 래퍼가 흡수 |

배출을 `finally` 에 둔 이유: 네이티브 호출이 던져도 그 전에 큐에 쌓인 것은 보여야 한다. 실패한 호출이야말로 알림이 중요한 순간이다.

배출이 자기 예외로 호출의 예외를 덮지 않도록, `DrainNativeAlerts` 쪽이 자기 실패를 삼킨다(`DllNotFoundException` / `EntryPointNotFoundException`).

**중복 배출 없음 확인** — 배출 호출은 래퍼 한 곳뿐이다:

```
86:  public string GetDisplayVersion() => InvokeNative(XpeDisplayNative.GetVersion);
97:      InvokeNative(() => ApplyDisplayPipelineCore(rawFrame, settings));
196:     InvokeNative(() =>
209:  private T InvokeNative<T>(Func<T> call) => NativeAlertDrain.InvokeWithDrain(call, DrainNativeAlerts);
213:  private void DrainNativeAlerts()
```

## 3. 회귀 3건 + 실큐 (카드 2·3항)

`step2-regression.log`:

```
통과!  - 실패: 0, 통과: 13, 건너뜀: 0, 전체: 13
```

| 케이스 | 확인 |
|---|---|
| `InvokeWithDrain_DrainsAfterTheCall` | (a) 호출 뒤 배출. 순서까지 단언(`["call","drain"]`) |
| `InvokeWithDrain_DrainsEvenWhenTheCallThrows` | (b) 예외 경로에서도 배출 1회, **호출의 예외가 그대로 전파** |
| `InvokeWithDrain_DrainsExactlyOncePerCall` | (c) 호출 1회당 배출 1회 (제네릭·void 양쪽) |

실큐 통합 테스트(C-22)는 그대로 통과했다 — `step2b-realqueue.log`: `실패 0 / 통과 4 / 건너뜀 0`. **건너뜀 0** 이 DLL 로드 성공의 증거다.

### 반증 (`step3-falsify.log`) — `finally` 를 없애 성공 경로에서만 배출

```
실패!  - 실패: 1, 통과: 8, 전체: 9
NativeAlertDrainTests.InvokeWithDrain_DrainsEvenWhenTheCallThrows [FAIL]
```

정확히 예외 경로 1건만 실패한다. 변형이 실제로 동작을 바꾸는 것을 확인했다(gate #14). 원복 후 재확인.

## 4. WPF 회귀 확인 (카드 범위 밖, 자발적)

`ApplyDisplayPipeline` 을 `Core` 로 쪼개 래퍼로 감쌌으므로 실행 경로가 바뀌었다. 실제 앱으로 확인했다 — `after-Native.json`:

```
Native: Passed=True | VoiPresetApplied=True | DisplayPipelineApplied=True | Alerts(초기/로드후/지운후)=1 1 0
```

C-23 이 되돌린 `Passed=true` 가 유지되고, 파이프라인도 정상 적용된다.

## 5. 실측 (verbatim)

```
dotnet build gui/ImageProcTest/ImageProcTest.csproj -c Debug -t:Rebuild
    경고 0개
    오류 0개

dotnet build clients/ImageProcTest.slnx -c Debug -t:Rebuild
    경고 0개
    오류 0개

dotnet test clients/ImageProcTest.IntegrationTests/ImageProcTest.IntegrationTests.csproj -c Debug
통과!  - 실패:     0, 통과:   150, 건너뜀:     1, 전체:   151
```

Baseline 귀속: C-23 최종 `0/147/1/148`(`GUI-C-23/step5-final.log`) → 증가 3 = 신규 회귀 3건. 카드가 예상한 +3 과 일치한다.

## 6. 미검증 (Gaps)

- **배출이 실제 앱에서 새 지점에 붙어 동작하는 것은 관측하지 못했다.** #134 이후에도 생산자는 `enhance_basic` 하나뿐이고(C-22 §6) gui 는 그 모듈을 호출하지 않는다. 즉 이 카드는 **배선의 사정거리를 넓혔을 뿐, 휴면 상태 자체를 깨지 못한다** — 생산자가 생기거나 gui 가 그 모듈을 부르기 전까지는 그대로다.
- `CanUseNative` 의 `NativeLibrary` 호출 3건은 감싸지 않았다(정적, 인스턴스 없음). 그 경로에서 네이티브가 알림을 쌓아도 배출되지 않는다 — 현재 그 API 들은 알림을 만들지 않지만 확인한 것은 아니다.
- 배출이 던지는 경우는 테스트하지 않았다. `DrainNativeAlerts` 가 두 예외만 삼키므로 그 밖의 예외(예: `AccessViolation`)는 `finally` 에서 호출의 예외를 덮는다.
- 동작 단위가 아니라 P/Invoke 단위로 알림이 쌓이는 경우(파이프라인 중간 호출이 알림을 만들고 다음 호출이 큐를 채우는 상황)의 순서는 확인하지 않았다.
- CI 실행 결과는 아직 없다.

## 7. 잔여 위험 (Residual risk)

- **새 네이티브 호출을 `InvokeNative` 없이 추가하면 조용히 빠진다.** 이 카드가 없앤 것은 "호출 지점마다 배선" 이지 "배선을 잊을 가능성" 이 아니다. 기계적 가드(예: 모든 `XpeDisplayNative.` 호출이 래퍼 안에 있는지 검사)는 만들지 않았다.
- 파이프라인 전체가 하나의 배출 단위라, 중간에서 던지면 그때까지 쌓인 알림만 나온다. 이후 호출이 만들었을 알림은 존재하지 않으므로 손실은 아니다.
- `ApplyDisplayPipelineCore` 분리로 공개 표면은 그대로지만 스택이 한 겹 깊어졌다.

## 부록 — 사용한 명령

```bash
git fetch origin main && git merge origin/main            # → dd9cc90
grep -n "XpeCommonNative\.\|XpeDisplayNative\.\|NativeLibrary" gui/ImageProcTest/Services/RealXpeBackend.cs
export PATH="/c/Program Files/dotnet:$PATH"
dotnet test … --filter "…NativeAlertDrainTests|…AlertQueueJunctionTests"   # 회귀·반증
ImageProcTest.exe --automation-raw … --automation-report …                 # WPF 회귀 확인
dotnet build gui/ImageProcTest/ImageProcTest.csproj -c Debug -t:Rebuild
dotnet build clients/ImageProcTest.slnx -c Debug -t:Rebuild
dotnet test clients/ImageProcTest.IntegrationTests/ImageProcTest.IntegrationTests.csproj -c Debug
```
