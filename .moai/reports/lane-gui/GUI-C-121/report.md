# GUI-C-121 — 경고가 한 번만 뜨게 한다 (#173)

## 1. 주장

세 조건이 각각 성립한다.

| 상황 | 동작 | 잰 방법 |
|---|---|---|
| 이번 실행이 **구조함** | `appsettings.json` 을 기본값으로 새로 씀 → 다음 실행 조용 | E2E |
| **이미 구조본이 있어 안 옮김** | 파일 그대로, 경고 반복 | E2E |
| **구조 실패** | 아무것도 덮어쓰지 않음 | Integration |

그리고 **가장 값진 것은 반증이 처음에 안 터진 일**이다 — 시험이 세 번째 조건을 재지 못하고 있었다.

## 2. 증거

### (a) 구현 전 빨강

```
E2E          : The rescuing run left no settings file behind at all.        (실패 1 / 통과 3)
Integration  : The rescuing load left no settings file behind.              (실패 1 / 통과 1)
```

### (b) 초록 — 세 조건

**① 구조 성공 → 다음 실행 조용**

```
first  launch status: '... The original file was kept at '...unreadable-20260919-113537'.'
file after rescue: 1482 bytes
second launch status: 'Backend initialized: v0.0.0-mock'      ← 경고 없음
rescue 는 여전히 1개, 내용은 사용자의 원본
```

**② 이미 구조본 있음 → 그대로, 경고 반복**

```
second launch status: '... An earlier failure already rescued your original settings, kept at '...'.'
File.ReadAllText(path) == CorruptAgain       ← 두 번째 파일은 건드리지 않음
```

**③ 구조 실패 → 원본 그대로**

```
preserved='' fromEarlier=False
file after: { "voiWindowCenter": 1234, "laneBAlgorithm":      ← 원본 그대로
Directory.GetFiles(.unreadable-*) → 비어 있음
```

### (c) 반증 — **처음에 안 터졌고, 그것이 이 카드의 핵심 발견이다**

교체 조건을 `if (true)` 로 바꿔 무조건 덮게 했는데 **시험이 통과했다.**

원인: 세 번째 조건을 `FileShare.None` 잠금으로 재고 있었는데, **그 잠금은 이동뿐 아니라 쓰기도 막는다.** 즉 무조건 덮는 빌드에서도 덮기가 실패했고, 시험은 "원본이 그대로" 를 확인하며 통과했다. **가드가 아니라 잠금이 원본을 지키고 있었다.**

두 번째 시도(`FileShare.ReadWrite`)도 같은 이유로 실패했다 — 다른 핸들이 열려 있으면 `File.WriteAllText` 도 막힌다.

성립한 방법: **목표 이름을 디렉터리로 선점**한다. 그러면 `File.Move` 만 실패하고 설정 파일 자체는 읽기·쓰기가 자유롭다. 이름이 초 단위 타임스탬프이므로 현재 초와 다음 초 **둘 다** 막았다.

이 방법으로 주입 상태에서 반증이 터졌다:

```
WITH_INJECTION_EXIT=1
Assert.Equal() Failure: Strings differ
Expected: "{ "voiWindowCenter": 1234, "laneBAlgorith"···   ← 원본이 덮였다
```

원복 후 다시 초록.

### (d) 전체

```
BUILD_EXIT=0
Mock  E2E 전체   : 통과 118, 건너뜀 21, 실패 0 (4m 51s)
IntegrationTests : 통과 254, 건너뜀 1, 실패 0
```

## 3. baseline 귀속

- 주입 전 `AppSettingsService.cs` 사본 보관 → 복원, `FALSIFICATION` 0건 확인 후 전체 실행.
- 모든 수치는 이 워크트리·이 실행.

## 4. 미검증 (Gaps)

- **`TryWriteDefaults` 가 실패하는 경로**는 재지 않았다. 실패하면 다음 실행이 다시 경고할 뿐 잃는 것은 없다고 코드 주석에 적었지만, 실행하지는 않았다.
- 디렉터리 선점은 **실제 사용자 환경에서 흔한 상황이 아니다.** `File.Move` 를 실패시키는 현실적 원인(다른 프로세스의 핸들)은 쓰기도 함께 막으므로, 이 시험이 재는 것은 **가드의 논리**이지 그 현실적 상황에서의 동작이 아니다. 다만 그 사실 자체를 (c)에서 측정했다.
- 초 경계 경합은 두 개(현재·다음 초)를 막아 줄였을 뿐 **원리적으로 제거하지는 못했다.** 실행이 2초를 넘겨 걸리면 이 시험은 조용히 통과한다 — 관측된 실행 시간은 수십 ms 다.
- 경고가 "한 번만" 뜬다는 것은 **연속 두 실행**으로만 확인했다. 세 번째 실행 이후는 재지 않았다.

## 5. 잔여 위험

- 구조 성공 뒤 사용자가 기본값 파일을 보고 **"설정이 초기화됐다"** 만 인지하고 구조본을 안 찾을 수 있다. 경고가 한 번만 뜨므로, 그 한 번을 놓치면 경로를 다시 볼 곳이 없다 — 로그에는 남는다.
- 이 카드는 경고 반복을 **①에서만** 없앴다. ② 상황(미처리 구조본이 있는 채로 계속 실패)은 의도적으로 매번 경고한다.

🗿 MoAI
