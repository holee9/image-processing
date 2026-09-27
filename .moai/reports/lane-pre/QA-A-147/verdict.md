# QA-A-147 (#212) — 이름을 실재에 맞추고, 단언을 실측값으로 좁혔습니다

판정: 카드 §3 의 세 갈래 중 **①(단언 좁히기) + ②(화소 단언 추가) + ③(이름 고치기)** 를 함께.
하나만 고르지 않은 이유를 §1 에 적습니다.

---

## 0. 먼저 — 제 앞선 보고의 오류를 정정합니다

`QA-A-146` 보고서와 이슈 코멘트에 **"정량 단언은 `EXPECT_LE(ms, 500)` 하나"** 라고 적었습니다. **틀렸습니다.**

그 단언은 `FullPipelineSmallImage` 가 아니라 **`DISABLED_PipelinePerformance3072x3072`**
(`test_integration.cpp:123`)에 있습니다. `DISABLED_` 는 기본 실행에서 **돌지 않습니다.**

정정하면 상황은 더 나쁩니다:

- `FullPipelineSmallImage` 에는 정량 단언이 **하나도 없었습니다** (제가 보고한 "하나" 가 아니라 0건)
- `500 ms` 인수 기준은 **아무도 돌리지 않는 시험** 안에 있습니다 (`DISABLED_` 는 이 모듈 시험 전체에서 이 하나뿐)

원인은 제 쪽입니다 — 같은 파일에서 `EXPECT_LE` 를 grep 으로 잡고 **어느 `TEST` 블록 안인지 확인하지
않았습니다.** `#171`("가드를 읽고 인용한다")과 같은 형태입니다: 줄을 인용할 땐 감싼 블록까지 봐야 합니다.

`DISABLED_` 는 이 카드 범위 밖이라 손대지 않았고, 사실만 남깁니다.

---

## §1 먼저 물을 것 — 이 시험이 답하려던 질문

### 측정 (추측 아님)

세 단계의 `rc` 를 프로브로 찍었습니다.

| 맥락 | S4 offset | S5 gain | S6 defect |
|---|---|---|---|
| 이 시험 단독 | `-6` | `-6` | `-6` |
| 전체 기본 순서 | `-6` | `-6` | `-6` |
| shuffle seed 9 | `-6` | `-6` | `-6` |

`-6` = `XPE_ERR_NOT_INITIALIZED` (`xpe_error.h:53`). **다른 값은 한 번도 나오지 않았습니다.**
이 시험은 `xpe_preprocess_init()` 도 `xpe_calib_load_*` 도 부르지 않습니다(전수 0건).

### blame — 허용 집합은 의도가 아니라 방어였습니다

`git blame test_integration.cpp:88` → `f3ccb0ec` (2026-09-10),
"XPE_ERR_CALIB_NOT_LOADED 도입 + 보정 함수 초기화 검사 추가".

그 커밋의 diff:

```
-        EXPECT_TRUE(rc == XPE_OK || rc == XPE_ERR_NOT_INITIALIZED);
+        EXPECT_TRUE(rc == XPE_OK || rc == XPE_ERR_NOT_INITIALIZED || rc == XPE_ERR_CALIB_NOT_LOADED);
```

새 오류 코드가 생기자 **그 코드를 실제로 내는 자리인지 보지 않고 모든 자리에 더했습니다.**
실측상 `CALIB_NOT_LOADED` 도 `XPE_OK` 도 `UNSUPPORTED_FORMAT` 도 여기서는 **도달 불가**입니다.
`#196`("가드가 감싸던 것보다 오래 산다")과 같은 형태입니다.

### 판단 — 왜 셋을 함께 했는가

카드는 하나를 고르라 했고, 고르지 **못한** 것이 아니라 **세 가지가 서로 다른 결함**이라 함께 했습니다.

| 갈래 | 이 시험의 상태 | 조치 |
|---|---|---|
| ① 단언이 실재보다 넓다 | 도달 불가한 갈래 3개 | 실측값 하나로 좁힘 |
| ② 화소 단언 0건 | **항등 구현도 통과** | 종료 상태 화소 단언 추가 |
| ③ 이름이 약속을 과장 | 7단계 중 3개가 무조건 무동작 | 개명 |

**캘리브레이션을 싣지 않은 이유**는 중복이기 때문입니다. 단계별 화소 정확성은
`test_golden_reference.cpp` 가 이미 화소 단위로 봅니다 — offset(`:161`·`:192`), gain(`:254`),
ghost(`:349`·`:364`), temp(`:463`). 여기서 캘리브레이션을 실으면 그것을 다시 하면서, 전역 상태가
없던 시험에 전역 상태를 들입니다. **이 시험이 유일하게 보는 것은 단계 간 조립**(버퍼·형식 인계와
순서)이고, 이름을 거기에 맞췄습니다.

`FullPipelineSmallImage` → **`UncalibratedPipelineComposes`**.
무엇을 보고 무엇을 안 보는지, 어디가 대신 보는지를 시험 머리에 적었습니다.

---

## §2 전수 — 한 건이 아니라 **14곳 / 7파일**

`modules/preprocess/tests/` 전수, `EXPECT_TRUE(rc == … || rc == …)`.
**대조군**: 같은 디렉터리에서 `ASSERT_EQ(XPE_OK` 는 30개 파일에서 잡힙니다 — 검색이 눈멀지 않았습니다.

| 파일 | 건수 | 형태 | 처리 |
|---|---|---|---|
| `test_integration.cpp` `:88`·`:94`·`:101` | 3 | `XPE_OK` + 무동작 코드, 주석 없음 | **좁힘** |
| `test_boundary.cpp` `:48`·`:58`·`:67`·`:79` | 4 | 위와 **같은 형태** (주석 없음) | **좁힘** |
| `test_preprocess_degraded.cpp` `:127`·`:176`·`:223` | 3 | `XPE_OK` 포함이지만 **의도가 주석에 명시** | 보고만 |
| `test_xpe_preprocess_memleak.cpp` `:85` | 1 | 이 파일은 모듈 초기화를 **부릅니다**(3건) | 보고만 |
| `test_xcal_reader.cpp` `:93`, `test_xpe_calib_check_expiry.cpp` `:145`, `test_xpe_calib_load.cpp` `:141` | 3 | `IO_FAILED \|\| CONFIG_INVALID` — **`XPE_OK` 없음** | 해당 없음 |

**같은 결함은 7건**(integration 3 + boundary 4)이고 둘 다 고쳤습니다. 나머지 7건은 다릅니다:

- **`test_preprocess_degraded`** 는 허용을 **문서화**합니다 — *"Accept NOT_INITIALIZED … or OK (if a prior
  test populated g_calib). Both paths are graceful degradation."* 즉 **앞선 시험의 전역 상태 누수를
  일부러 용인**합니다. 그 용인 자체가 위생 시험(`test_global_state_hygiene`)과 긴장 관계라 재검토할
  값이 있지만, **의도된 결정을 뒤집는 것은 다른 판단**이라 손대지 않았습니다 — 리더 몫으로 올립니다.
- **`test_xpe_preprocess_memleak`** 은 모듈 초기화를 부르므로 `XPE_OK` 가 실제로 날 수 있습니다.
  좁히려면 먼저 재야 하고, 이 카드에서 재지 않았습니다.
- 뒤의 3건은 **두 오류 코드 중 하나**를 받는 형태로, 무동작을 성공으로 읽는 문제가 없습니다.

---

## §3 고친 내용

1. **단언 좁히기** — `test_integration` 3곳·`test_boundary` 4곳을 `EXPECT_EQ(XPE_ERR_NOT_INITIALIZED, rc)` 로.
   부수 효과가 의도적입니다: **다른 시험이 전역 초기화를 흘리면 여기서 빨개집니다.**
2. **화소 단언 추가** — 실행 종료 시 `gainBuf` 전체가 초기값 `1.0f` 인지. 근거는 세 단계가 거부해
   쓰지 않고, ghost 프레임 0 은 문서화된 정확 통과(`test_golden_reference.cpp:349`), 1×1 binning 은
   무동작이기 때문입니다. **이전에는 모든 단계가 항등 함수여도 통과했습니다.**
3. **개명** — `UncalibratedPipelineComposes`. 살아있는 코드·문서의 옛 이름 인용은 전수 0건
   (과거 보고서 2곳은 실행 기록이라 그대로 둡니다).

헤더 주석(`preprocess_api.h`)에 `#209` 버퍼 별칭 계약을 리더가 쓴 SPEC(`d1fbf3f`)과 같은 표현으로
넣었습니다 — 카드 §5 의 이월 항목입니다.

---

## §4 반증 — 각 단계에 손상 주입

**주입 A — 실제로 도는 단계가 화소를 건드림.** `xpe_ghost_correct` 진입 직후 `img->data[0] = -12345.0f`:

```
test_integration.cpp(150): error: Expected equality of these values:
    Which is: 1
    Which is: 0
[  FAILED  ] Integration.UncalibratedPipelineComposes
```

(값이 `-12345` 가 아니라 `0` 인 것은 tier1 이 음수를 0 으로 clamp 하기 때문입니다 — `ghost_correct.cpp:108`.)
**새 화소 단언이 잡습니다. 이전 판이라면 초록이었습니다.**

**주입 C — 단계가 아무 일도 안 하고 성공을 보고함.** `xpe_offset_correct` 에서 런타임-거짓 가드로
`return XPE_OK`:

```
test_integration.cpp(119): error: Expected equality of these values:
    Which is: 0
[  FAILED  ] Integration.UncalibratedPipelineComposes
```

**좁힌 `rc` 단언이 잡습니다.** 옛 허용 집합은 `XPE_OK` 를 **첫 항으로** 받았으므로 이 주입에
초록이었습니다 — `QA-A-146b` 센티넬이 통과한 것과 같은 구멍입니다.

둘 다 `BUILD_EXIT=0`, DLL 타임스탬프 갱신 확인, 확인 후 제거(`grep -c` 0건).

**정직하게 — 안 잡히는 것**: `QA-A-146b` 의 센티넬(`defect_correct` 의 memcpy `else` 분기)은 **지금도
이 시험을 빨갛게 만들지 못합니다.** 그 단계가 초기화 검사에서 먼저 거부해 센티넬 줄에 도달하지
않기 때문입니다. 좁힌 단언이 이 시험에서 defect 경로의 내부 동작을 보게 만들지는 않습니다 —
그것은 `DefectCorrectTest` 의 몫이고, 이 시험은 조립만 봅니다.

---

## 검증

| 항목 | 결과 |
|---|---|
| 빌드 | `BUILD_EXIT=0` |
| 전체 ctest | **757 / 757** (`CTEST_EXIT=0`) |
| 한 프로세스 네 순서 (기본 + seed 1/2/9) | 각각 `ran=688`, `PASSED 680`, 실패 0 |
| 좁힌 단언의 순서 민감도 | 네 순서 모두 초록 — 현재 전역 위생이 실제로 지켜지고 있다는 뜻 |

## 미검증 / 잔여 위험

- **`test_preprocess_degraded` 3건과 `test_xpe_preprocess_memleak` 1건은 재지 않았습니다.** 프로브를
  넣지 않았고, "의도적으로 보인다"는 주석과 모듈 초기화 호출 유무로 판단했습니다. 좁힐 수 있는지는
  재야 압니다 — **리더 판단 필요**.
- 좁힌 단언은 전역 위생이 계속 지켜진다는 전제입니다. 앞선 시험이 초기화를 흘리면 여기가
  빨개지는데, 그것은 이 시험의 결함이 아니라 흘린 쪽의 결함입니다. 다만 **실패가 여기에 나타난다**는
  점은 기록해 둡니다.
- `DISABLED_PipelinePerformance3072x3072` 는 손대지 않았습니다. `500 ms` 인수 기준이 실행되지 않는
  상태이며, 이 카드 범위 밖입니다.

🗿 MoAI
