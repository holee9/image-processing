# GUI-C-132 — `#196` 알림이 화면에 온다. 그리고 **래치는 GUI 에서 유지되지 않는다**

## 1. 결과

**(a) 화면에 왔다.** `#198` 이 열린 이유였던 두 알림 중 나머지 하나다.

```
[18:11:35.594] ALERT WARN NATIVE_ALERT: nonlinearity correction did nothing: no LUT is
loaded and panel.linear is not "false", so the frame passed through unchanged; load a LUT
with xpe_calib_load_nonlin_lut() if this panel needs correcting (issue #196)
```

**(b1) 그런데 "조건당 한 번" 이 아니다 — 3프레임에 3건이다.** 카드가 지시한 단언은
"한 번" 이었고, **재 보니 틀렸다.** 원인은 GUI 배선이 아니라 **재무장 경로가 하나 더
있는 것**이다(§3).

**(b2) 대조군은 살아 있다.** 같은 실행에서 `#194` 클램프가 프레임마다 왔다(3건).
`#196` 이 "한 번" 이 아니라 "매번" 이므로 이 대조군의 역할은 카드가 예상한 것과
반대가 됐다 — **큐가 죽지 않았다는 것을 보이는 대신, pre 가 우려한 축출이 실제로
일어날 조건임을 보인다.**

| 측정 | 값 |
|---|---|
| 프레임 | 3 |
| 전처리 로그 줄 | 6 |
| `#196` 무동작 줄 | **3** (프레임당 1) |
| `#194` 클램프 줄 | **3** (설계상 프레임당 1) |
| 로그 전체 | 44 줄 |

## 2. 왜 이제야 쟀나 — 그리고 **낡은 바이너리를 먼저 잡았다**

스테이징된 `xpe_preprocess.dll` 이 `2026-09-19 17:10` 이고 `QA-A-140`(`0875afe`)은
`17:59` 였다 — **49분 낡은 빌드**다. 그대로 쟀다면 알림이 안 떠서 "화면까지 오지
않는다" 로 **오독**했을 자리다.

갱신하고 귀속을 확인했다:

```
CI run 35435692495, head 7d32053
git merge-base --is-ancestor 0875afe 7d32053  → yes   (QA-A-140 포함)
staged xpe_preprocess.dll  2026-09-19 17:10 → 2026-09-26 17:36
```

**문자열 프로브는 판별에 쓰지 않았다.** `nonlinearity correction did nothing` 은
`QA-A-127` 때 들어온 문자열이라 `QA-A-140` 전후 양쪽 빌드에 있다 — 있다는 것이 아무것도
말해 주지 않는다. 판별은 **커밋 조상 관계**로 했다.

(같은 종류의 오류를 `GUI-C-131` 에서 실제로 저질렀고 이번에 정정했다 —
`REQ-DISP-029` 는 주석에만 있어 어떤 빌드에도 리터럴로 들어가지 않는데 그 부재를
신선도 근거로 썼다. **주석에만 있는 식별자는 바이너리 프로브의 대상이 될 수 없고,
대조군이 그 결함을 잡아 주지 못한다.**)

## 3. 발견 — **재무장 경로가 세 번째로 있다**

모듈 주석은 재무장 지점을 **둘**로 적는다: LUT **적재**(`xpe_calib_load_nonlin_lut.cpp:76`)
와 **해제**(`:102`). 전역 상태를 직접 세어 확인했다 — `nonlin_noop_reported` 를 쓰는
곳은 그 둘과 래치 자신뿐이다.

```
modules/preprocess/src/nonlinearity_correct.cpp:179  if (g_calib.nonlin_noop_reported)
modules/preprocess/src/nonlinearity_correct.cpp:183      g_calib.nonlin_noop_reported = true
modules/preprocess/src/xpe_calib_load_nonlin_lut.cpp:76  = false   (적재)
modules/preprocess/src/xpe_calib_load_nonlin_lut.cpp:102 = false   (해제)
```

**그런데 통째로 지우는 곳이 따로 있다.**

```
modules/preprocess/src/preprocess.cpp:77   (xpe_preprocess_shutdown 안)
    g_calib = CalibrationData{};
```

`CalibrationData` 를 새로 대입하므로 `nonlin_noop_reported` 도 **`false` 로 돌아간다.**
이름에 래치가 나오지 않으니 검색으로는 보이지 않는다.

그리고 **GUI 는 실행마다 그것을 부른다** (`GuiPreprocessRunner.cs:87`, `finally` 안):

```
init → load offset/gain/defect → stages → finally shutdown
```

**그래서 GUI 는 프레임마다 래치를 무장하고 버린다.** `#196` 이 프레임당 한 번 뜨는 것이
이 구조의 결과다.

## 4. §5 답 — **GUI 는 "재무장을 못 하는 호스트" 가 아니다. 반대다**

pre 의 물음은 *"프로파일을 바꾸며 `xpe_calib_unload_nonlin_lut()` 를 안 부르는 호스트는
재보고를 못 받는다 — GUI 가 그 호스트입니까"* 였다.

**부르지 않는다는 것은 맞다. 그런데 결론은 반대다.**

```
files searched: 777
control — load_offset: 2, load_gain: 2, load_defect_map: 2
claim   — load_nonlin_lut: 0, unload_nonlin_lut: 0
call site passes config = null
```

GUI 에는 비선형 LUT 의 **적재·해제 호출부가 아예 없다**(offset·gain·defect 만 적재).
**부재 주장이므로 대조군을 짝지웠다** — 같은 검색·같은 파일 집합에서 GUI 가 실제로
부르는 로더 셋이 잡힌다. 안 잡히면 0건은 "없다" 가 아니라 "엉뚱한 곳을 봤다" 다.

**실제 사용자에게 어떤 뜻인가** — 두 가지가 겹친다.

1. **재보고 유실은 없다.** 적재 경로가 없으므로 GUI 에서 무동작 조건은 **한 번도
   해소되지 않는다.** 보고할 두 번째 조건 자체가 없다.
2. **대신 축출 위험이 산다.** `shutdown` 이 매 실행 래치를 풀기 때문에 프레임당 1건이
   쌓인다. 64건 상한 FIFO 이므로 **연속 64프레임이면 다른 알림이 전부 밀려난다 —
   `#194` 클램프 포함.** pre 가 억제를 넣어 막으려던 바로 그 상황이고, GUI 경로에서는
   막히지 않았다.

**이것은 모듈 소유 영역이라 고치지 않았다.** 결정은 lead·pre 몫이다.

## 5. 반증 — 대상만 사라지는지

`RealXpeBackend.DrainNativeAlerts` 에서 **`#196` 만** 걸러 냈다(전체 배선을 끊으면
대조군까지 죽어 무엇이 사라졌는지 흐려진다).

```
#196 no-op lines: 0     ← 사라졌다
#194 clamp lines: 3     ← 그대로
preprocess lines: 6     ← 그대로
Lines carrying ALERT: 4
```

**대상만 0 이 됐고 나머지는 움직이지 않았다.** 그 줄이 네이티브 큐에서 오는 것이지
GUI 가 스스로 쓴 문장이 아님을 이것이 보인다.

주입과 원복을 둘 다 확인했다(주입이 조용히 안 먹은 채 초록을 읽는 함정):

```
falsification injected / grep -c "FALSIFICATION (#196" → 1
원복 후 → 0, git diff --stat 비었음 (HEAD 와 바이트 동일), 빌드 오류 0
```

## 6. 빌드·시험

```
BUILD_EXIT=0
IntegrationTests        : 통과 264, 건너뜀 1, 실패 0  (전체 265)
Mock E2E 전체           : 통과 124, 건너뜀 25, 실패 0 (7m 16s)
Native (#196 + #194)    : 통과 2, 실패 0
```

건너뜀이 24 → 25 인 것은 이번에 더한 네이티브 전용 1건이다 — Mock 백엔드에는 네이티브
전처리 단계가 없어 **재는 것이 없으므로 건너뛴다**(실패가 아니라).

공개 헤더와 P/Invoke 선언은 바꾸지 않았다(`git diff --name-only`: E2E 시나리오 1건,
통합시험 1건). 그래서 `check_header_docs.py` 는 돌리지 않았다.

## 7. 단언을 "한 번" 으로 두지 않은 이유

카드는 *"조건이 처음 생겼을 때 한 번"* 을 지시했다. 측정은 **매 프레임**이었다.
1 로 고정하면 **오늘 그냥 빨강**이고, 빨강은 무엇이 틀렸는지 말해 주지 않는다.

그래서 **측정값(= 프레임 수)에 고정하고, 숫자가 바뀌면 무슨 뜻인지를 단언 메시지에
적었다**:

- `1` 이 되면 → 래치가 실행을 넘어 살아남기 시작한 것(= 의도대로 고쳐진 것). 시험이
  빨개지므로 **알아차린다.** 그때 숫자를 고치는 대신 모듈 소유자에게 알리라고 적었다.
- `2×프레임` 이면 → 이중 발화.

**단언이 의도가 아니라 측정을 가리키고, 의도와 어긋나는 지점을 시험 안에 적어 둔 것이다.**

## 8. 미검증 (Gaps)

- **재무장(카드 (b) 셋째)은 확인하지 못했다.** GUI 에 LUT 적재·해제 경로가 없어
  **화면 쪽에서 수단이 없다.** 못 했다고 적는다.
- **64프레임을 돌려 실제 축출을 보지 않았다.** 3프레임에서 프레임당 1건인 것을 재고
  상한 64를 읽어 **추론**했다. 축출이 실제로 일어나는 것은 안 봤다.
- **`panel.linear` 를 `"false"` 로 주는 경우를 재지 않았다.** GUI 는 `config` 를 null 로
  넘기므로 그 분기(다른 알림, `:88`)는 이 경로에서 닿지 않는다.
- **`xpe_preprocess_shutdown` 이 래치를 지우는 것을 소스로 읽었고, 그것이 원인이라는
  것을 개입으로 확인하지는 않았다.** 모듈 소유가 아니라 바꿔서 사라지는지 볼 수 없었다
  — `#163` 에서 쓴 양방향 방식을 여기서는 쓰지 못했다.
- 프레임 3개, 합성 1024×1024 한 장뿐이다. 손목(3072²)에서는 재지 않았다.

## 9. 잔여 위험

- 측정값에 고정한 단언은 **모듈이 고쳐지면 빨개진다.** 의도한 설계지만, 사정을 모르는
  사람이 숫자만 고칠 수 있다 — 그래서 단언 메시지에 "숫자를 고치지 말고 알리라" 를
  적었다.
- `#194` 건수 869,755/1,048,576(83%)은 **합성 프레임의 성질**이다. 대조군으로 쓰기에는
  충분하지만 임상 빈도를 말하지 않는다.
- Native E2E 를 두 시나리오만 돌렸다. 네이티브 전체 회귀는 CI 몫이다.

🗿 MoAI
