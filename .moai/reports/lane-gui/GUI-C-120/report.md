# GUI-C-120 — 최초 구조본을 덮지 않는다 (#173)

## 1. 주장

1. `.unreadable-*` 이 이미 있으면 **새로 만들지 않는다.** 파일 수가 1개로 고정된다.
2. 남는 1개는 **최초** 구조본 — 사용자의 진짜 설정 쪽이다.
3. 두 번째 알림이 **"이전 실패가 이미 구조해 둔 원본이 여기 있다"** 를 경로와 함께 말한다.
4. 구현 전 빨강, 구현 후 초록, 조건 제거 시 다시 빨강.

## 2. 증거

### (a) 구현 전 빨강 — 실제로 쌓인다

```
RED_EXIT=1   통과: 2   실패: 1
A second failure left 2 rescued files; they accumulate with nobody to remove them.
rescued after second:
  ...appsettings.json.unreadable-20260919-112339
  ...appsettings.json.unreadable-20260919-112340
```

타임스탬프가 달라 **둘 다 생긴다**는 것을 실행으로 확인했다.

### (b) 초록 — 1개, 그리고 그 1개가 최초 것

```
first  launch status: '... The original file was kept at '...unreadable-20260919-112549'.'
second launch status: '... An earlier failure already rescued your original settings,
                        kept at '...unreadable-20260919-112549'.'
rescued after second: ...appsettings.json.unreadable-20260919-112549   ← 1개, 첫 번째 경로
```

시험은 세 가지를 단언한다 — 개수가 안 늘어난다 · 남은 파일의 **내용이 첫 번째 원본**이다 · 두 번째 문구가 **그 파일 이름**과 `earlier` 를 담는다.

### (c) 문구가 달라지는 이유

두 경우는 **사실이 다르다.** 재실패 때 이 실행은 **아무것도 옮기지 않았고**, 경로가 가리키는 것은 **이전 실패가 구조한 파일**이다. 여기서 "원본을 보관했다" 라고 말하면 거짓이고, 사용자는 방금 잃은 설정이 그 안에 있다고 믿게 된다.

```
1회차: The original file was kept at '<경로>'.
2회차: An earlier failure already rescued your original settings, kept at '<경로>'.
```

### (d) 반증

`var earlier = ExistingRescue();` → `string? earlier = null;`

```
RED_EXIT=1   통과: 2   실패: 1
A second failure left 2 rescued files; they accumulate with nobody to remove them.
```

### (e) 전체

```
BUILD_EXIT=0
Mock  E2E 전체   : 통과 117, 건너뜀 21, 실패 0 (4m 49s)
IntegrationTests : 통과 252, 건너뜀 1, 실패 0
```

## 3. baseline 귀속

- 주입 전 `AppSettingsService.cs` 사본 보관 → 복원, `FALSIFICATION` 0건 확인 후 전체 실행.
- 모든 수치는 이 워크트리·이 실행.

## 4. 미검증 (Gaps)

- **왜 최초가 값진가는 이 카드에서 재지 않았다.** 근거는 C-118 의 측정(다음 저장이 1482 바이트로 덮는다)과 리더 판단이다. "두 번째 구조본이 실제로 기본값에 가깝다" 를 이 카드가 직접 재지는 않았다 — 시험은 **첫 번째가 남는다**만 단언한다.
- 여러 개가 이미 쌓인 트리(이 규칙 이전 상태)에서 **가장 오래된 것을 고르는** 동작은 코드에 있으나(`OrderBy` ordinal) 실행으로 재지 않았다. 이름이 정렬 가능한 타임스탬프 형식이라는 전제에 기댄다.
- `ExistingRescue` 가 예외를 만나는 경우(디렉터리 권한)는 재지 않았다. `null` 을 돌려 **기존 동작(새로 구조)** 으로 떨어지는데, 그쪽이 안전한 방향인지는 판단이지 측정이 아니다.
- 사용자가 구조본을 지운 뒤 다시 실패하면 새로 구조된다 — 카드가 맞는 동작이라 했고, 그대로 동작할 것으로 보이나 재지 않았다.

## 5. 잔여 위험

- **읽을 수 없는 `appsettings.json` 자체는 그 자리에 남는다.** 재실패 때 옮기지 않기로 했으므로, 사용자가 저장을 누르기 전까지 매 실행이 같은 경고를 낸다. 반복 경고가 "치우는 사람 없음" 의 다른 형태가 될 수 있다 — 다만 파일이 쌓이지는 않는다.
- 1개 고정은 **정리 주체를 없앴지, 파일을 없앤 것이 아니다.** 사용자가 그 1개를 영원히 안 지우면 그대로 남는다.

🗿 MoAI
