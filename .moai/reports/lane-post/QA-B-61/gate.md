# QA-B-61 게이트 보고서 — per-call config 의 미지 키를 설정당 한 번

**카드**: QA-B-61 (#145)
**레인**: Lane B (`xpe-post`, `dev/postprocess`)
**증거 경로**: `.moai/reports/lane-post/QA-B-61/`
**커밋 1건**: `7e181d1`

---

## 1. 설계 결정 두 가지 — 고른 것과 근거

### 1.1 비교 단위 = **미지 키 이름의 집합** (config 문자열 전체가 아님)

| 후보 | 판단 |
|---|---|
| **미지 키 이름 집합**(정렬 후 결합) | **채택.** 값이 프레임마다 바뀌는 호출자 — `{"step_size": 0.31}` → `0.32` → … — 에게 문자열 비교는 **매 프레임 다시 울린다.** 애초에 없애려던 홍수가 그대로 남는다 |
| config 문자열 전체 | 기각. 위와 같은 이유. 비용은 더 싸지만 **가장 흔한 호출 형태에서 목적을 달성하지 못한다** |

비용도 따져 봤다: 키 이름은 **메시지를 만들려고 이미 모으고 있으므로**, 추가되는 것은 짧은
문자열 몇 개의 정렬과 한 번의 비교뿐이다. 측정으로 확인한 것은
`VaryingTheValueEachFrameStillWarnsOnce` 다 — 값을 50번 바꿔도 **경고 1건**.

### 1.2 상태 위치 = **`thread_local`**, 호출부가 소유하고 참조로 전달

| 후보 | 판단 |
|---|---|
| **`thread_local`** (호출부 소유) | **채택.** 교차 가시성이 0 이므로 스레드가 서로를 방해할 수 없다 |
| 모듈 전역(+뮤텍스) | 기각. **스레드 둘이 서로의 기억을 지운다** — A 가 경고 → B 의 다른 config 가 기록을 덮음 → A 의 다음 프레임이 다시 울림. 스레드가 둘일 때마다 **프레임당 홍수가 되돌아온다.** 카드가 지적한 바로 그 형태다 |

**요구 문구와의 긴장 — 해소하지 않고 적는다.**

> REQ-ADV-090 (`spec.md:321`): "All processing functions **shall** be reentrant with
> independent caller-supplied buffers. **No global mutable state shall be modified during
> processing calls.** The `g_initialized` flag is the only shared state…"

- **재진입의 취지는 유지된다**: 결과가 이 기억에 의존하지 않고, 스레드가 서로의 것을 볼 수 없다.
- **문구의 글자와는 어긋날 수 있다**: `thread_local` 은 정적 저장 기간을 갖고, 처리 호출 중에
  수정된다.
- **다만 "설정당 한 번" 이라는 동작은 호출을 넘어 사는 기억을 필연적으로 요구한다.**
  선택지는 그 기억을 **스레드가 공유하느냐**뿐이고, 이것은 공유하지 않는다.

두 성질을 **주장이 아니라 측정**으로 남겼다(§3 의 (9)(10)). **판정은 리더 몫으로 남긴다** —
문구를 이 레인에서 재해석하지 않는다.

> **사후 정정 2건 (2026-09-12, 리더 판정 뒤).**
> 1. **요구 번호가 틀렸다.** 위에 인용한 문구는 `REQ-ADV-090` (Thread Safety) 이고,
>    `REQ-ADV-032` 는 NaN/Inf 요구다. 본문을 확인하지 않고 번호를 옮겨 적어 생긴 오류로,
>    #156 의 `voi_lut.cpp` 오인용과 같은 계열이다. 여기와 `enhance_advanced_helpers.cpp`,
>    `test_config_warning_once.cpp` 세 곳을 고쳤다.
> 2. **판정이 났고 SPEC 이 바뀌었다(`d52a5da`).** 옛 문구는 지키려던 성질로 다시 쓰였다 —
>    **스레드 간 공유 금지 + 출력이 다른 호출의 상태에 의존하지 않을 것.** 위 인용은
>    개정 전 문구이므로 현재 SPEC 과 다르다. 허용은 좁고, **뮤텍스로 보호된 모듈 전역은
>    보호된다는 이유만으로 해당하지 않는다.** 아래 §6 의 첫 Gap 은 이로써 닫혔다.

### 1.3 정상 config 는 기억을 **지운다**

지우지 않으면 오타를 고쳤다가 되살린 호출자가 **두 번째에는 아무 소리도 못 듣는다.**
`FixingThenReintroducingTheTypoWarnsAgain` 이 그것을 단언한다.

---

## 2. 배선 (`7e181d1`)

`warn_unconsumed_keys_once()` 를 `enhance_advanced_helpers.cpp` 에 두고 세 호출부에서
각자의 `static thread_local std::string` 을 넘긴다.

| 진입점 | 알려진 키 | 중첩 |
|---|---|---|
| `xpe_multiscale_process` | `num_levels` · `levels` · `edge_gain` · `texture_gain` · `flat_gain` · `noise_threshold` | `"mfp"` 객체 내부도 판정 |
| `xpe_fractional_process` | `iterations` · `step_size` · `safety` | — |
| `xpe_detect_collimation` | `sensitivity` · `min_area_ratio` · `border_margin` | — |

**반환 코드는 바꾸지 않았다.** `fractional` 의 SAF-100 금지 키 검사는 **거절**이고 이것은
**보고**다 — 다른 기전이라 손대지 않았다.

---

## 3. RED → GREEN (`_green.log`, BUILD=0, 10/10)

| # | 단언 | 결과 |
|---|---|---|
| 1 | 미지 키 config 로 **100프레임 → 경고 1건** | OK |
| 2 | 정상 config **100프레임 → 0건** | OK |
| 3 | `NULL`·`{}` 각 50프레임 → 0건 | OK |
| 4 | 중첩 `mfp` 스키마는 **내부 키**로 판정, `mfp` 자체는 보고 안 함 | OK |
| 5 | **다른** 미지 키는 다시 울림 | OK |
| 6 | 고쳤다가 되살리면 다시 울림 | OK |
| 7 | 값만 50번 바뀌면 여전히 **1건** | OK |
| 8 | `fractional`·`collimation` 도 각 30프레임 → 1건 | OK |
| 9 | **화소 동일** — 경고 경로가 결과를 바꾸지 않는다 | OK |
| 10 | **스레드 둘이 서로의 경고를 지우지 않는다**(각 40프레임, 각자 1건) | OK |

(9)(10)이 §1.2 의 재진입 주장을 측정으로 바꾼 자리다.

### 3.1 테스트에서 한 번 잘못 읽을 뻔했다

처음 실행에서 두 케이스가 실패했다. **결함이 아니라 설계대로 동작한 결과였다** —
기억이 **스레드당 프로세스 수명**이므로, 앞 케이스가 쓴 키는 뒤 케이스가 돌 때 이미
기억돼 있다(gtest 는 모든 케이스를 한 스레드에서 돌린다).

케이스마다 고유한 키를 쓰도록 고치고 **그 경위를 테스트 파일에 남겼다.** 이것을
"중복 억제가 너무 세다" 로 읽고 제품을 고쳤다면, **테스트의 착오를 제품 변경으로 갚는**
형태가 됐을 것이다.

---

## 4. 반증 — 피해 크기가 수치로 나왔다 (`_falsify.log`)

중복 억제를 무효화(`if (signature == lastWarned && false) return;`)하고 재실행:

```
===BUILD=0===
100 frames, unknown key: 64 alert(s)
  alert queue overflow: 37 alert(s) dropped
[  FAILED  ] HundredFramesWithOneUnknownKeyWarnOnce
[  FAILED  ] NestedMfpSchemaJudgesInnerKeys / ADifferentUnknownKeyWarnsAgain /
             VaryingTheValueEachFrameStillWarnsOnce / FractionalAndCollimationWarnOncePerConfig
[       OK ] HundredFramesWithValidConfigAreSilent / NullAndEmpty… / Pixels…
```

세 가지가 한 번에 나온다:

1. **"큐가 한 가지 사실의 복사본으로 차고 다음 진짜 경고를 밀어낸다" 가 측정이 됐다** —
   100프레임에서 64건이 쌓이고 **37건이 버려졌다**.
2. **알림 큐 용량이 64 라는 것이 함께 확인됐다** — B-60 이 "미측정" 으로 남긴 Gap 이다.
3. **침묵 쪽 단언 3건은 그대로 통과한다**(정상 config·NULL/빈·화소 동일) — 중복 억제를
   꺼도 헛경고는 생기지 않으므로, 두 축이 서로 다른 것을 보고 있다는 증거다.

반증 뒤 원복했다.

---

## 5. Baseline 귀속 (Baseline-attribution)

| 항목 | 값 | 근거 |
|---|---|---|
| 배선 진입점 | **3개** | `7e181d1` |
| GREEN | BUILD=0, **10/10** | `_green.log` |
| 100프레임 → 경고 | **1건** | `_green.log` |
| 정상 config 100프레임 | **0건** | `_green.log` |
| 반증 | BUILD=0, **64건 + 37건 드롭** | `_falsify.log` |
| 알림 큐 용량 | **64** (실측) | `_falsify.log` |
| 이전 ctest | 512 / 222 / 177 | QA-B-60 `_verify.log` |
| 현재 ctest | **522 / 222 / 177** (신규 10건) | `_verify.log` |
| 빌드 경고 | 0 | `_verify.log` |

---

## 6. 미검증 (Gaps)

- **`thread_local` 의 요구 적합성은 판정하지 않았다**(§1.2). 재진입의 **취지**가 유지되는
  것은 측정했고, **문구**와의 관계는 리더 판정 대상으로 남긴다.
- **스레드 수가 많을 때의 경고 수를 재지 않았다.** 설계상 같은 config 라도 **스레드당 한 번**
  울리므로, 스레드 풀이 크면 그 수만큼 나온다. 프레임 수와 무관하다는 점이 이 카드의 목표였고
  그것은 달성됐지만, **스레드 수에는 비례한다.**
- **알림 큐가 이미 차 있는 상태에서의 동작을 재지 않았다** — 용량 64 와 오버플로 보고가
  있다는 것만 확인했다.
- **타입 불일치(`"timeout_ms": "500"` 형태)는 손대지 않았다** — 카드가 범위 밖으로 명시했고,
  그것은 경고가 아니라 타입 진단이 할 일이다.
- **`xpe_enhance_advanced_init` 은 여전히 배선하지 않았다** — B-60 판정대로 (b) 문서화 쪽으로
  정해졌고, 코드 주석에 "미사용" 을 적지 말라는 지시도 지켰다.

---

## 7. 잔여 위험 (Residual-risk)

- **"경고를 한 번 들었다" 가 "고쳤다" 는 뜻이 아니다.** 같은 config 로 계속 돌면 이후로는
  조용하다 — 의도이지만, 로그를 뒤늦게 보는 사람에게는 **한 줄만 남는다.**
- **"경고가 없다" 가 "설정이 적용됐다" 는 뜻은 여전히 아니다**(B-60 잔여 위험, 카드가 기록으로
  받은 문장). 이름과 타입이 맞아도 값이 버려지는 경로는 이 경고가 보지 못한다 —
  그것은 의존성 전수(B-58 계열)가 보는 것이다. **두 장치가 서로 다른 것을 본다.**
- **스레드당 기억은 스레드가 끝나면 사라진다.** 프레임마다 새 스레드를 쓰는 호출자에게는
  중복 억제가 무력하다 — 그런 호출 형태가 실재하는지는 확인하지 않았다.
- 커밋은 push 전까지 미푸시 유일본이다.

---

## 부록 — 증거 파일

| 파일 | 내용 |
|---|---|
| `_env.bat` / `_verify.bat` | 환경 + 세 프리셋 재빌드·전체 ctest |
| `_b61.bat` / `_green.log` | 10건 BUILD=0 |
| `_falsify.log` | 중복 억제 무효화 — 64건 + **37건 드롭** |
| `_verify.log` | 최종 522 / 222 / 177, 경고 0 |
