# GUI-C-119 — 원본을 먼저 지키고, 그 다음 알린다 (#173)

## 1. 주장

1. **(A)** 읽기에 실패하면 원본을 `appsettings.json.unreadable-<타임스탬프>` 로 **옮긴다.** 저장은 막지 않는다.
2. **(B)** 실패가 뷰모델까지 간다. 화면 문구에 세 가지가 다 있다 — 못 읽었다 · 기본값으로 시작했다 · 원본은 여기 있다.
3. **(C)** 그 확인을 **앱을 띄워서** 했다. 양방향이다.
4. 반증 두 방향 모두 빨강이 된다.

## 2. 증거

### (A) 원본이 남는다

`AppSettingsService.Load` 가 실패 경로에서 `PreserveUnreadable()` 을 거쳐 파일을 옮기고 그 경로를 돌려준다. 반환 타입을 `AppSettings` → `SettingsLoadResult(Settings, PreservedOriginalPath)` 로 바꿨다 — **실패가 담길 자리가 없던 것이 C-118 에서 잰 조용함의 원인**이었다.

저장을 막지 않은 이유는 카드의 판단 그대로다: 막으면 사용자가 나갈 길이 없고, 치우면 저장은 자유롭고 되돌릴 수 있다.

E2E 가 실제 파일을 확인한다:

```
preserved: C:\Users\...\xpe-c119-b20e.../appsettings.json.unreadable-20260919-111009
Assert.Equal(원본 내용, File.ReadAllText(preserved))   → 통과
```

`.bak` 자동 순환은 만들지 않았다. 이 동작은 **읽기 실패 시 한 번**이다.

### (B) 화면 문구 — 세 요소

```
Your saved settings could not be read, so this session started from defaults.
The original file was kept at '...\appsettings.json.unreadable-20260919-111009'.
```

- ① 못 읽었다 — `could not be read`
- ② **기본값으로 시작했다** — `started from defaults`
- ③ **원본은 여기 있다** — 경로 전체

`Alerts`(`SETTINGS_UNREADABLE`, WARN) · `StatusText` · `Logs` 세 곳에 한 번씩. 생성자에서 `InitializeBackend()` **뒤에** 말한다 — 그 앞에서 말하면 같은 생성자 안에서 지워진다(GUI-C-62·63 에서 측정된 순서).

### (C) 앱을 띄운 양방향 E2E

`UnreadableSettingsScenarios`. C-118 이 "화면에 아무것도 없다" 를 코드로만 뒷받침했다고 스스로 적었으므로, 이번 확인은 **실제 실행 파일**을 깨진 파일 앞에 세웠다.

```
corrupt launch status: 'Your saved settings could not be read, so this session
  started from defaults. The original file was kept at '...unreadable-20260919-111009'.'
good    launch status: 'Backend initialized: v0.0.0-mock'
```

정상 파일 쪽은 **아무 말도 안 한다**는 것과, 옆에 치워진 파일이 **0개**라는 것까지 단언한다. 이게 없으면 "항상 뜨는 알림" 과 구별되지 않는다.

이를 위해 `--automation-settings <path>` 스위치를 더했다. 자동화 모드는 원래 임시 디렉터리의 기본값에서 시작하므로, 이 스위치가 없으면 **이 경로는 코드를 읽어야만 주장할 수 있다** — 이 이슈가 계속 없애 온 종류의 주장이다.

`AutomationRunSelectionConsumptionTests` 의 제외 목록에 사유를 적어 넣었다(이 스위치는 *무엇을 선택하는가* 가 아니라 *설정이 어디 있는가* 이므로 `ApplyRunSelection` 의 대상이 아니다). 가드를 느슨하게 만들지 않고, 가드가 제공하는 **명명된 제외** 방식을 썼다.

### (D) 반증 — 두 방향

**① (A) 를 되돌림** — `File.Move` 제거

```
RED_EXIT=1   통과: 1   실패: 1
Expected exactly one preserved original, found 0.
```

**② (B) 를 되돌림** — `StatusText = message;` 제거

```
RED_EXIT=1   통과: 1   실패: 1
Assert.Contains() Failure: Sub-string not found
String: "Backend initialized: v0.0.0-mock"
```

두 방향이 **서로 다른 사유**를 낸다 — 원본이 안 남는 것과 말하지 않는 것.

### (E) 전체

```
BUILD_EXIT=0
Mock  E2E 전체   : 통과 116, 건너뜀 21, 실패 0 (4m 46s)
IntegrationTests : 통과 252, 건너뜀 1, 실패 0
```

C-118 의 측정 케이스는 살려 두고 `result.FailedToRead` 단언을 더했다 — 그 케이스가 재던 "알 방법이 없다" 는 이제 사실이 아니므로, **계약은 그대로 두고 단언을 뒤집었다**(C-115 의 U-03 과 같은 성질).

## 3. baseline 귀속

- 주입 전 `AppSettingsService.cs`·`MainWindowViewModel.cs` 사본을 보관하고 그 파일로 복원. 복원 뒤 `FALSIFICATION` 0건.
- 모든 수치는 이 워크트리·이 실행.

## 4. 미검증 (Gaps)

- **잘린 JSON 한 가지로만 E2E 를 돌렸다.** 잘못된 타입·UTF-16 은 단위 수준(C-118 측정)에서만 확인했다. 세 경로가 `Load` 안에서 같은 지점으로 모이므로 같이 동작할 것으로 보지만, **재지 않았다**.
- **`PreserveUnreadable` 이 실패하는 경우**(권한 없음·파일 잠김)는 재지 않았다. 코드는 `null` 을 돌려주어 "원본은 여기 있다" 를 **말하지 않게** 되어 있지만, 그 경로를 실행해 보지는 않았다.
- **같은 초에 두 번 실패하는 경우**의 이름 충돌 회피(`-1`, `-2` 접미사)는 코드에 있으나 재지 않았다.
- 알림이 **한 번만** 뜨는지는 단일 실행에서만 봤다. 재시작 때마다 남은 깨진 파일이 없으므로 반복되지 않을 것으로 보지만, 두 번 띄워 확인하지는 않았다.
- `--automation-settings` 가 **비자동화 실행**(실제 사용자)에 미치는 영향은 없다 — 스위치가 없으면 기존 경로 그대로다. 이것도 코드로만 확인했다.

## 5. 잔여 위험

- 치워 둔 파일은 **아무도 지우지 않는다.** 반복해서 깨지면 `.unreadable-*` 이 쌓인다. 상시 정리는 이 카드가 아니라고 판단했다(카드의 "`.bak` 자동 순환 금지" 와 같은 선).
- 문구는 영어다. 화면의 다른 문구와 같은 상태이지만, 한국어 UI 를 쓰는 사용자에게는 이 경고도 영어로 보인다.

🗿 MoAI
