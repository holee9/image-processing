# GUI-C-21 — gui 앱이 네이티브 알림 큐를 소비해 표시한다 (#110 표시 경로)

- 카드: GUI-C-21 · Refs #110 · 워크트리 `D:/workspace-github/xpe-gui` / `dev/gui`
- 선행: C-20 (`ab04a11`) 뒤, main `95f36a7` 병합 상태
- **결과: clients 재빌드 경고 0/오류 0 · gui 재빌드 경고 0/오류 0 · dotnet 실패 0 / 통과 143 / 건너뜀 1 / 전체 144**

---

## 1. 주장 (Claim)

1. `RealXpeBackend` 가 표시 파이프라인 호출 직후 네이티브 알림 큐를 동기 배출해 `_alerts` 에 넣고 `xpe_clear_alerts()` 한다 — UI(`MainWindowViewModel.DrainBackendTelemetry`)가 이미 읽는 컬렉션이다.
2. `BUFFER_TOO_SMALL` 은 버퍼를 키워 1회 재시도하고, 그래도 실패하면 `"[unreadable alert]"` 로 **넣는다**(버리지 않는다).
3. 심각도는 낮추지 않는다 — 미지의 값·읽기 실패·유실 알림 모두 `ERROR`.
4. **`AlertDisplayFormatter` 는 `clients` 에 그대로 두었다.** gui 가 링크 소스로 소비한다(§3).
5. 회귀 6건 + 반증 1회로 확인했다.

## 2. 구현

| 파일 | 변경 |
|---|---|
| `gui/ImageProcTest/Services/NativeAlertDrain.cs` (신규) | 순수 배출 로직. 리더를 델리게이트로 받아 네이티브 없이 검증 가능 |
| `gui/ImageProcTest/Services/RealXpeBackend.cs` | `DrainNativeAlerts()` + `ReadNativeAlert()`. `ApplyDisplayPipeline` 의 summary 직전(이미지 free 이전)에서 호출 |
| `gui/ImageProcTest/Services/Native/XpeDisplayInterop.cs` | `xpe_get_pending_alert_count` / `xpe_get_pending_alert` / `xpe_clear_alerts` 바인딩 3건 추가 — `xpe_common.h` 시그니처 그대로, 네이티브 변경 없음 |
| `gui/ImageProcTest/ImageProcTest.csproj` | `AlertDisplayFormatter.cs` 링크 |
| `clients/…IntegrationTests.csproj` | `AlertEntry.cs` + `NativeAlertDrain.cs` 링크 |

배출은 델리게이트 뒤에 두었다. `NativeAlertDrain` 이 P/Invoke 를 직접 부르면 이 테스트 어셈블리에서 돌릴 수 없다 — `XpeCommonApi` 정적 생성자가 DllImport 리졸버를 두 번째로 등록하며 던진다(C-13 실측). 결정 로직만 떼어 두면 그 벽을 넘지 않고 검증된다.

`DrainNativeAlerts` 는 `DllNotFoundException` / `EntryPointNotFoundException` 을 삼킨다. 알림 큐가 없는 옛 `xpe_common.dll` 로도 표시 파이프라인 자체는 계속 돌아야 한다 — 알림 배출 실패가 이미지 처리를 죽이면 안 된다.

## 3. 포맷터 배치 결정 (카드 요구 보고 항목)

**`clients/ImageProcTest/Services/AlertDisplayFormatter.cs` 에 그대로 두고, gui 가 `<Compile Include Link>` 로 컴파일에 넣는다.**

근거: 두 앱 모두 `WinExe` 이고 라이브러리가 없다. ProjectReference 로 exe 를 참조하면 gui 출력에 clients 앱 어셈블리가 딸려 들어간다. 이 저장소는 이미 같은 문제를 링크 소스로 풀고 있다(테스트 프로젝트가 앱 파일 9개를 링크). **원본은 한 곳, 저자도 한 곳** 이 유지된다 — 옮겼다면 clients 쪽 `RealXpeCommonBackend`(네이티브 알림 API 를 만지는 다른 소비자)가 역방향 링크를 하게 된다.

## 4. 증거 (Evidence)

### 회귀 6건 (`step2-regression.log`)

| 케이스 | 확인 |
|---|---|
| `EmptyQueue_ProducesNoEntries` | 0건 — 리더가 아예 호출되지 않음(`Assert.Fail` 리더로 고정) |
| `TwoAlerts_PreserveQueueOrderAndSeverity` | 순서 보존 + INFO/WARN 매핑 |
| `OverflowAlert_RendersAsError_WithFormattedMessage` | 큐가 Info(0) 로 줘도 `ERROR`, 코드 `ALERT_QUEUE_OVERFLOW`, 문장 렌더 |
| `BufferTooSmall_RetriesOnceWithALargerBuffer` | 시도 버퍼가 정확히 `[256, 8192]` — 1회 재시도 |
| `UnreadableAlert_IsKeptAsAPlaceholder_NotDropped` | 두 번 다 실패해도 1건이 남음 |
| `UnknownSeverity_IsNotLowered` | 99 / -1 → `ERROR` |

```
통과!  - 실패: 0, 통과: 6, 건너뜀: 0, 전체: 6
```

### 반증 (`step3-falsify.log`) — 읽기 실패 항목을 버리게 변형

```
실패!  - 실패: 1, 통과: 5, 전체: 6
NativeAlertDrainTests.UnreadableAlert_IsKeptAsAPlaceholder_NotDropped [FAIL]
```

변형이 실제로 동작을 바꾼다(항목 1건 → 0건). 정확히 그 가드의 케이스만 실패한다. 원복 후 재확인함.

### 최종 실측 (verbatim)

```
dotnet build clients/ImageProcTest.slnx -c Debug -t:Rebuild
    경고 0개
    오류 0개

dotnet build gui/ImageProcTest/ImageProcTest.csproj -c Debug -t:Rebuild
    경고 0개
    오류 0개

dotnet test clients/ImageProcTest.IntegrationTests/ImageProcTest.IntegrationTests.csproj -c Debug
통과!  - 실패:     0, 통과:   143, 건너뜀:     1, 전체:   144
```

## 5. Baseline 귀속

- 직전 기준: GUI-C-20 최종 `0/137/1/138` (`GUI-C-20/step5-final.log`, 이번 세션 실측)
- 이번: `0/143/1/144` — 증가 6 = 신규 회귀 6건.
- gui 프로젝트는 `clients/ImageProcTest.slnx` 에 없어 **따로 빌드**했다. C-19 까지의 "앱 재빌드" 는 clients 만이었다.

## 6. 미검증 (Gaps)

- **네이티브 큐로 실제 배출해 본 적이 없다.** 회귀는 전부 가짜 리더다. `xpe_get_pending_alert` 의 실제 반환 규약(빈 큐일 때 코드, 인덱스 범위 밖 동작)은 관측하지 않았다 — A-28 병합 뒤 leader 가 확인할 접합부다.
- **WPF 앱을 띄워 알림 목록을 보지 않았다.** 빌드와 유닛까지다. `_alerts` → UI 전달은 기존 `DrainBackendTelemetry` 경로를 그대로 쓴다는 코드 독해에 근거하며, 실행 관측은 아니다.
- **배출 지점은 `ApplyDisplayPipeline` 하나뿐이다.** `LoadRawImage` / `CreateVoiPreset` 뒤에는 배출하지 않는다 — 카드가 "처리 호출 직후" 로 한정했고 전면 배선은 범위 밖이다. 그 경로에서 네이티브가 알림을 쌓으면 다음 파이프라인 호출까지 표시되지 않는다.
- `RealXpeCommonBackend`(clients 쪽 진단 문자열, 개수만)는 손대지 않았다.
- CI 실행 결과는 아직 없다.

## 7. 잔여 위험 (Residual risk)

- `xpe_clear_alerts()` 는 배출 성공 여부와 무관하게 호출된다. 읽지 못한 항목도 큐에서 사라지지만, 그 자리에 `[unreadable alert]` 가 남으므로 **"알림이 있었다" 는 사실은 보존**된다. 본문은 잃는다.
- 재시도는 1회다. 8192자를 넘는 메시지는 `[unreadable alert]` 가 된다 — 계약상 그런 메시지는 없다고 보지만 확인하지 않았다.
- 링크 소스는 컴파일 사본을 늘린다. `AlertDisplayFormatter` 는 이제 3곳(clients 앱 / gui 앱 / 테스트)에서 컴파일된다. 원본은 하나라 드리프트는 없지만, 파일을 옮기면 링크 3개가 동시에 깨진다.
- 배출은 UI 스레드에서 동기로 돈다. 알림이 매우 많으면 파이프라인 호출이 그만큼 늘어진다 — 폴링 타이머 금지 조항 때문에 이 구조를 택했다.

## 부록 — 사용한 명령

```bash
export PATH="/c/Program Files/dotnet:$PATH"
dotnet test … --filter "FullyQualifiedName~NativeAlertDrainTests"     # 회귀·반증
dotnet build clients/ImageProcTest.slnx -c Debug -t:Rebuild
dotnet build gui/ImageProcTest/ImageProcTest.csproj -c Debug -t:Rebuild
dotnet test clients/ImageProcTest.IntegrationTests/ImageProcTest.IntegrationTests.csproj -c Debug
```
