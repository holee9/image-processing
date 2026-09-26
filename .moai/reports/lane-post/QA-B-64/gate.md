# QA-B-64 게이트 보고서 — #142 D5·D4 를 "일한다" 로 단언

**카드**: QA-B-64 (#142) · **레인**: Lane B (`xpe-post`, `dev/postprocess`)
**커밋 1건**: `885cddb` · **증거**: `.moai/reports/lane-post/QA-B-64/`

---

## 1. 먼저 — 5건이 아직 있는지 확인했다

카드 지시대로 **이슈 본문을 현재 상태로 읽지 않았다.** 각각을 코드에서 확인했고,
**셋은 이미 처리돼 있었다.**

| # | 본문(2026-09-11 기준) | 현재 상태 | 근거 |
|---|---|---|---|
| **D1** 빈 이미지 계약 | `enhance_basic` 6함수에서 계약이 갈림 | **미확인 — 다음 카드로** | §5 |
| **D2** include guard 중복 | 두 헤더가 같은 가드 | **해소됨** | `enhance_advanced_api.h:17` = `XPE_ENHANCE_ADVANCED_API_COMPAT_H`, `xpe_enhance_advanced_api.h:1` = `XPE_ENHANCE_ADVANCED_API_H` — 다르다 |
| **D3** 미등록 중복 소스 2개 | CMake 에 없고 중복 정의 포함 | **삭제됨** | `git log --diff-filter=D` → `723d076` "빌드에 등록되지 않은 중복 소스 2개 제거 (QA-B-40 D3)". `src/` 에 남은 것은 `xpe_enhance_advanced.cpp` 하나 |
| **D4** fallback 무효과 | 저장만 하고 읽는 경로 없음 | **그대로 있다** | §3 |
| **D5** cross-DLL free | 공유 CRT 가정 | **측정·문서화는 됐으나 전제가 단언이 아니었다** | §2 |

---

## 2. D5 — 왕복은 통과하는데 전제는 아무도 보지 않았다

### 2.1 상태

QA-B-40 이 이미 처리했다: `dumpbin` 으로 두 DLL 이 공유 UCRT 를 쓰는 것을 재고,
결과를 `presentation_lut.cpp:53-60` 주석에 적고, 상시 검사
`PresentationLutCrossDllTest.CommonAllocatedBufferSurvivesDisplayConversion` 을 붙였다.

**그런데 그 상시 검사는 왕복만 본다.** `xpe_alloc_image`(common) →
`xpe_apply_presentation_lut`(display 가 free 하고 재할당) → `xpe_free_image`(common) 이
`XPE_OK` 를 내는지 확인한다.

**이것은 보이는 것보다 약하다.** 어느 모듈이 static CRT 로 바뀌면 이 케이스는 **깨끗하게
실패하지 않는다** — 남의 힙 포인터를 `free` 하는 것은 미정의 동작이라 크래시하거나
**조용히 깨지면서 PASS 를 찍는다.** 그러면 검사는 **있으되 쓸모없어진다.**

이 세션에서 반복해서 만난 형태다: **있다 ≠ 일한다.**

### 2.2 한 것 — 전제를 단언으로

주석에 적힌 전제를 기계 단언으로 바꿨다. 로드된 두 모듈의 **PE 임포트 테이블을 프로세스
안에서 직접 읽어** 확인한다(파일 I/O 없음, 외부 프로세스 호출 없음):

1. 두 모듈 다 힙 제공자를 임포트해야 한다 — **static CRT 는 이 임포트가 아예 없다**
2. 그 제공자가 **서로 같아야** 한다 (둘 다 비어 있지 않은 것만으로는 부족)
3. 임포트 테이블을 못 읽으면 `ASSERT` 로 먼저 멈춘다 — **빈 집합이 "없음" 으로 읽히는 것을
   막는 대조군**

### 2.3 CRT 재실측 — 물려받지 않았다 (`_d5_crt.log`)

```
xpe_common.dll   VCRUNTIME140.dll / VCRUNTIME140_1.dll / api-ms-win-crt-heap-l1-1-0.dll
xpe_display.dll  VCRUNTIME140.dll / api-ms-win-crt-heap-l1-1-0.dll
```

B-40 의 주장은 2026-09-16 현재도 성립한다. **다만 이번에는 주석이 아니라 테스트가 이것을
붙잡고 있다.**

### 2.4 GREEN (`_d5.log`)

```
===BUILD=0===
[       OK ] PresentationLutCrossDllTest.CommonAllocatedBufferSurvivesDisplayConversion
[  INFO ] xpe_common heap providers:  api-ms-win-crt-heap-l1-1-0.dll
[  INFO ] xpe_display heap providers: api-ms-win-crt-heap-l1-1-0.dll
[       OK ] PresentationLutCrossDllTest.BothModulesResolveTheHeapThroughTheSameCrt
2 tests ran. ===EXIT=0===
```

### 2.5 반증 — 왕복은 이 문제를 볼 수 없다 (`_d5_falsify.log`)

대상을 힙 임포트가 없는 모듈(`ntdll.dll`)로 바꿔 힙 불일치를 흉내 냈다:

```
===BUILD=0===
[       OK ]   CommonAllocatedBufferSurvivesDisplayConversion      ← 그대로 통과
[  FAILED  ]   BothModulesResolveTheHeapThroughTheSameCrt          ← 전제만 실패
1 FAILED TEST
```

**이것이 이 카드의 요지다.** 왕복 케이스는 힙이 갈려도 통과한다. 전제 케이스만 잡는다.
원복했다.

---

## 3. D4 — 기존 테스트 5건이 "있다" 쪽만 단언한다

### 3.1 상태

`test_ai_fallback.cpp` 상단의 5건은 `xpe_ai_set_fallback_mode` 가 `1`·`0`·`42`·`-1`·`1000`
에 `XPE_OK` 를 내는 것을 확인한다. 전부 통과하고, 합쳐 놓으면 **기능이 검증된 것처럼 보인다.**
확인하는 것은 하나다 — **세터가 존재하고 성공을 보고한다.**

값이 무엇을 하는지는 별개이고 답은 "아무것도" 다. `parseConfig`(`ai.cpp:211`)와 이 세터
(`ai.cpp:649`)가 `state->fallbackMode` 에 쓰고, **읽는 코드가 없다**(`:415` 는 라우팅을
설명하는 주석).

같은 파일의 `ConfidenceThresholdDefaultIs06` · `ConfidenceThresholdInRange` 도 같은 형태다 —
**상수가 상수와 같은지**를 확인할 뿐 그 값이 쓰이는지는 말하지 않는다. (B-62 가 이미
`confidence_threshold` 의 읽는 코드가 0곳임을 기록했다.)

### 3.2 직접 재는 것은 stub 이 막는다 — 섞지 않았다

추론 경로가 라우팅 전에 `XPE_ERR_PROCESSING_FAILED` 로 조기 반환하므로, "두 설정에서
출력이 같다" 는 **"배선 안 됨" 과 "배선됐으나 도달 못 함" 양쪽과 똑같이 들어맞는다.**
B-62 가 같은 이유로 재지 않았다.

### 3.3 한 것 — stub 에 의존하지 않는 판별자

**모듈이 이 플래그를 되읽을 방법을 export 하지 않는다.** 호출자는 설정이 먹혔는지 확인할
수도, 기록할 수도, 분기할 수도 없다 — **추론 경로가 무엇을 하든 상관없이.** export
테이블에서 잰다.

```
[  INFO ] xpe_ai.dll exports 10 names; fallback read-backs: 0
[       OK ] AiFallbackTest.KnownDivergence_FallbackModeCanBeSetButNeverReadBack
[  INFO ] both settings returned -3 (XPE_ERR_PROCESSING_FAILED is the stub's early
           return) -- this case cannot separate unwired from unreached
[       OK ] AiFallbackTest.KnownDivergence_FallbackModeChangesNoObservableOutput
53 tests ran. ===EXIT=0===
```

행동 쪽 케이스는 **자기 한계를 로그에 같이 적었다** — 나중에 "플래그가 안 배선됐다" 는
증거로 인용되지 못하게.

### 3.4 반증 — 0건에 대조군 (`_d4_falsify.log`)

B-63 방법론 그대로. 세터 제외 조건(`name.find("set_") == npos`)을 빼면:

```
[  INFO ] unexpected fallback accessor: xpe_ai_set_fallback_mode
[  INFO ] xpe_ai.dll exports 10 names; fallback read-backs: 1
[  FAILED  ] KnownDivergence_FallbackModeCanBeSetButNeverReadBack
```

**세는 일이 실제로 일어나고 있다.** 원복했다.

---

## 4. Baseline 귀속 (Baseline-attribution)

| 항목 | 값 | 근거 |
|---|---|---|
| #142 5건 중 이미 해소 | **2건** (D2·D3) | §1, `723d076` |
| 이번에 단언 추가 | **2건** (D5·D4), 테스트 3건 | `_d5.log`, `_d4.log` |
| CRT 힙 제공자 (재실측) | 둘 다 `api-ms-win-crt-heap-l1-1-0.dll` | `_d5_crt.log` |
| D5 반증 | BUILD=0, **왕복 OK / 전제 FAILED** | `_d5_falsify.log` |
| `xpe_ai.dll` export | **10개**, fallback read-back **0개** | `_d4.log` |
| D4 반증 | BUILD=0, **0 → 1, FAILED** | `_d4_falsify.log` |
| 이전 ctest | 531 / 222 / 177 | QA-B-63 `_verify.log` |
| 현재 ctest | **532 / 224 / 177** | `_verify.log` |
| 빌드 경고 | 0 | `_verify.log` |

---

## 5. 미검증 (Gaps)

- **D1 을 보지 않았다.** 카드가 "다섯을 얕게 훑는 것보다 위험한 둘을 확실히" 라고 했고
  그대로 했다. 현재까지 본 것은 `enhance_basic/src/` 에서 `width == 0 || height == 0` 검사가
  **`exposure_index.cpp:75` 한 곳뿐**이라는 것 — 나머지 함수가 빈 이미지에서 무엇을
  반환하는지는 **실행으로 재지 않았다.** 정적 grep 은 후보이지 결론이 아니다. 다음 카드로.
- **D5 의 전제 단언은 Windows/PE 전용이다.** 다른 플랫폼 빌드가 생기면 이 케이스는 컴파일
  되지 않는다. 이 프로젝트는 MSVC 전용이라 지금은 문제가 아니지만 **이식성은 재지 않았다.**
- **전제 단언은 링크 시점이 아니라 런타임에 본다.** 테스트가 로드하지 않는 모듈 조합은
  검사되지 않는다 — 두 모듈 다 이 테스트 바이너리에 로드되므로 지금은 충분하지만,
  **전수는 아니다.**
- **D5 의 실제 힙 불일치는 만들어 보지 않았다.** 반증은 임포트가 없는 다른 모듈을 대입한
  것이고, `/MT` 로 실제 빌드해 왕복이 어떻게 깨지는지는 재지 않았다 — 미정의 동작을
  일부러 실행하지 않았다.
- **D4 의 "읽는 코드 0곳" 은 여전히 정적 사실이다.** `fallbackMode` 는 구조체 멤버라
  B-63 의 링커 스캔(UNDEF 참조)을 쓸 수 없다. export 판별자는 **ABI 표면**을 재는 것이지
  모듈 내부 사용을 재는 것이 아니다.

---

## 6. 잔여 위험 (Residual-risk)

- **D5 는 "지금 안전" 이지 "안전하게 설계됨" 이 아니다.** 소유가 DLL 경계를 두 번 넘는
  구조는 그대로이고, 안전은 **두 모듈이 같은 CRT 를 쓴다는 빌드 설정**에 걸려 있다.
  이번에 바뀐 것은 그 의존을 **주석에서 단언으로** 옮긴 것뿐이다. 소유 모듈이 free 하도록
  바꾸는 것은 별도 결정이고 여기서 하지 않았다.
- **단언은 CI 가 이 테스트를 돌릴 때만 일한다.** 정적 CRT 전환이 이 테스트를 빌드하지
  않는 경로로 들어오면 잡히지 않는다.
- **D4 는 고치지 않았다.** 세터는 여전히 `XPE_OK` 를 반환하고 값은 아무 데도 닿지 않는다.
  호출자에게는 **성공으로 보인다.** ONNX 실경로(#130)와 함께 결정할 사안이다.
- **기존 5건의 "있다" 테스트는 그대로 둔다.** 지우면 세터의 ABI 계약 검사가 사라진다.
  다만 **그 5건이 통과한다는 것이 fallback 이 동작한다는 뜻이 아니라는 것**이 이제
  같은 파일에 적혀 있다.
- 커밋은 push 전까지 미푸시 유일본이다.

---

## 부록 — 증거 파일

| 파일 | 내용 |
|---|---|
| `_env.bat` / `_verify.bat` / `_verify.log` | 세 프리셋, 532 / 224 / 177, 경고 0 |
| `_b64.bat` / `_d5.log` | D5 2건 BUILD=0, 힙 제공자 일치 |
| `_d5_crt.log` | CRT 임포트 **재실측**(물려받지 않음) |
| `_d5_falsify.log` | **왕복 OK / 전제 FAILED** — 왕복이 못 보는 것을 보여줌 |
| `_b64ai.bat` / `_d4.log` | D4 2건, export 10개 중 read-back 0개 |
| `_d4_falsify.log` | 0건의 대조군 — 제외 조건을 빼면 1건, FAILED |
