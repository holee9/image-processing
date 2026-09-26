# GUI-C-51 — 재획득 경로를 출력에 싣는다 (#136)

> **이번 실행이 쓴 네이티브**: run **34535620953** · head `83ffa7ae…` ·
> `xpe_preprocess.dll` md5 `9f81d2f07fb7f7e23e90c86332467105`.

- 카드: GUI-C-51 · Refs #136 · 워크트리 `D:/workspace-github/xpe-gui` / `dev/gui`
- 커밋 1건: `c7e921a` — 미푸시
- **재획득 분기를 실제로 밟았다.** 카드의 3항("확인 못 했다" 로 적는 경우)은 쓰지 않았다.
- **결과: Native 0/29/0/29(벽시계 69 s) · Mock 0/28/1/29 · 통합 0/180/1/181 · slnx 0/0**

---

## 1. 노트를 출력에 싣는다

`ReacquiredNote` 를 네 개의 `Measure` 헬퍼에서 전부 쓴다
(`SmokeScenarios` · `WorkflowScenarios` · `WorkflowMenuScenarios` · `ComparisonModeScenarios`).

두 가지를 의도했다:

- **본문 실행 전에 쓴다.** 시나리오가 던져도 기록이 남는다 — C-49 가 "이름이 남게 한다" 로
  세운 자리와 같은 성질이다. 출력은 trx 에 들어가고, leader 확인대로 CI 가 그 trx 를
  아티팩트로 보존한다.
- **비어 있으면 아무것도 쓰지 않는다.** 정상 실행(대부분)은 한 줄도 늘지 않는다.

## 2. 그 경로를 실제로 밟았다

카드가 "74회 정상 실행으로는 안 밟히니 주입 가능한 형태가 필요할 수 있다" 고 했고, 그 길을 썼다.

**시험용 이음매 2개**(둘 다 테스트 어셈블리 내부 `internal`, 앱 코드가 아니다):

| 이음매 | 왜 필요한가 |
|---|---|
| `SimulateUnreadableChecks` (카운터) | 첫 판독 검사만 실패시킨다. **플래그가 아니라 카운터**인 이유: 실제 관측된 모양은 "첫 요소는 실패, 재획득한 요소는 성공" 이다. 플래그면 둘 다 실패해 포기 경로를 시험하게 된다 |
| `SkipLeftoverSweep` | 스위트 중간에 만든 픽스처가 잔존 정리로 **다른 컬렉션의 앱을 죽인다** — C-36 이 실측한 경합. 전용 컬렉션으로도 분리해 앱 두 개가 동시에 살지 않게 했다 |

**모사되는 것은 방아쇠뿐이고 수리는 진짜다** — 진짜 앱, 진짜 기동, 진짜 프로세스 ID 재획득.
재획득이 동작하지 않으면 이 테스트가 실패한다.

### 실측 (verbatim)

```
reacquire note: 'The launched window did not support AutomationId (framework=Wpf);
                 re-located it by process id 22168.'
통과!  - 실패: 0, 통과: 2, 건너뜀: 0, 전체: 2
```

**`framework=Wpf` 로 나온다** — 모사에서는 건강한 WPF 요소의 검사를 강제로 실패시키므로, 노트가
보고하는 framework 는 그 요소의 실제 값이다. **실제 결함에서는 `Win32` 였다**(C-50 §1, 3건 모두).
노트가 그 차이를 그대로 드러내므로, 다음에 진짜로 채워지면 두 경우를 구분할 수 있다.

### 두 번째 테스트 — 건강한 경로가 조용한지

`ReadableWindow_IsKept_AndLeavesNoNote`. 이게 없으면 첫 테스트는 **노트를 무조건 채워도 통과**하고,
그러면 매 실행이 있지도 않은 결함을 경고하게 된다.

## 3. 반증 (빌드 결과 포함)

노트 기록을 약화시켰다(`ReacquiredNote = string.Empty`).

```
=== 빌드: 경고 0개 / 오류 0개          ← 빌드 정상
=== 테스트:
  실패 …WindowReacquireTests.UnreadableWindow_IsReplaced_AndTheRunSaysSo [547 ms]
   The fixture replaced the window but left no note, so a run that hit this defect would be
   indistinguishable from a healthy one — the gap GUI-C-51 exists to close.
실패!  - 실패: 1, 통과: 27, 건너뜀: 1, 전체: 29
```

**29건 중 그 가드 1건만** 실패한다. 원복 후 재확인.

`ReadableWindow_IsKept_AndLeavesNoNote` 는 이 약화에서 **통과한다** — 정상 경로는 원래 노트가
비어 있어야 하므로 옳은 동작이고, 두 단언이 서로 다른 것을 지킨다는 증거이기도 하다.

## 4. 실측 (verbatim)

```
XPE_E2E_BACKEND=Native XPE_NATIVE_DIR=<staging> dotnet test …E2ETests… --no-build
통과!  - 실패: 0, 통과: 29, 건너뜀: 0, 전체: 29 (기간 1 m 6 s / 벽시계 69 s)

dotnet test clients/ImageProcTest.E2ETests/… --no-build                        (Mock)
통과!  - 실패: 0, 통과: 28, 건너뜀: 1, 전체: 29 (16 s)

dotnet test clients/ImageProcTest.IntegrationTests/… -c Debug
통과!  - 실패: 0, 통과: 180, 건너뜀: 1, 전체: 181

dotnet build clients/ImageProcTest.slnx -c Debug -t:Rebuild
    경고 0개 / 오류 0개
```

Baseline 귀속: E2E 는 C-50 시점 **27건** → **29건**(재획득 2건). Native 벽시계 69 s 는
C-48~50 의 69~72 s 범위 안이다 — 새 테스트는 Mock 앱을 두 번 더 띄우지만 1초대다.
통합 181 불변.

**C-50 의 두 동작은 지시대로 건드리지 않았다** — 실패 시 원래 요소 유지, 재획득 상한 2초.

## 5. 미검증 (Gaps)

- **실제 결함 조건에서 노트가 채워지는 것은 여전히 못 봤다.** 밟은 것은 **모사된** 방아쇠이고,
  실제 `framework=Win32` 요소를 만난 적은 이 카드에서 없다. 그 차이는 노트 값에 드러난다(§2).
- **모사와 실제가 재획득 이후 같은 코드를 탄다는 것은 코드 구조상 그렇다.** 실행으로
  두 경로가 같음을 비교하지는 않았다.
- **노트가 trx 에 실제로 들어간 것을 확인하지 않았다.** 콘솔 출력에서는 봤고, xUnit 의
  `ITestOutputHelper` 가 trx 에 들어간다는 것은 C-49 에서 다른 값으로 확인했을 뿐 이 문자열로
  확인하지는 않았다.
- **왜 가끔 Win32 공급자를 받는지는 그대로 미결**이다(카드 범위 밖).
- **이음매가 정적 상태다**(§6).

## 6. 잔여 위험 (Residual risk)

- **`SimulateUnreadableChecks` 는 정적 필드다.** 테스트가 `finally` 에서 되돌리지만, 누수되면
  **다음 픽스처**가 재획득을 한 번 타고 엉뚱한 시나리오에 노트가 붙는다. 되돌림을 `finally` 에
  둔 이유가 이것이고, 병렬 실행이 켜지면 이 방식은 안전하지 않다(현재 어셈블리 직렬화, C-36).
- **`SkipLeftoverSweep` 도 정적이다.** 같은 이유로 누수 시 다음 픽스처가 잔존 정리를 건너뛴다 —
  조용히 느려지거나 이전 실행의 앱을 붙잡을 수 있다.
- **노트는 출력에만 있다.** 어떤 단언도 "정상 실행에서 노트가 비어 있어야 한다" 를 스위트 전체에
  걸어 두지는 않았다 — 전용 테스트 1건이 그 계약을 대표한다.
- 새 컬렉션이 앱을 2회 더 띄운다. Mock 기준 1초대이므로 게이트에 영향 없다(§4).

## 부록 — 사용한 명령

```bash
export PATH="/c/Program Files/dotnet:$PATH"
dotnet build clients/ImageProcTest.E2ETests/… -c Debug
dotnet test clients/ImageProcTest.E2ETests/… --no-build \
  --filter "FullyQualifiedName~WindowReacquireTests" --logger "console;verbosity=detailed"
# 반증: ReacquiredNote 기록을 string.Empty 로 약화
dotnet build clients/ImageProcTest.slnx -c Debug -t:Rebuild
dotnet test clients/ImageProcTest.E2ETests/… --no-build
XPE_E2E_BACKEND=Native XPE_NATIVE_DIR="$(pwd)/build/ci-common/bin" dotnet test …E2ETests…
dotnet test clients/ImageProcTest.IntegrationTests/… -c Debug
```
