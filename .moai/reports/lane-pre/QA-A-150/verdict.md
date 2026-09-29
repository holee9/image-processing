# QA-A-150 (#211/#197) — 35곳 옮기고, **덮지 못하는 자리 6종을 남겼습니다**

카드가 미리 지목한 셋(§1 `007` 알림 · §1 `008` 플래그 주체 · §2 `023`)은 **셋 다 다른 이유로 해소**됐고,
대신 **카드가 예상하지 않은 자리 넷**이 나왔습니다. orphan 0 을 목표로 삼지 않았습니다.

| | 전 | 후 |
|---|---|---|
| `modules/preprocess` orphan ID | **10** | **2** (`034`·`066` — 둘 다 의도적으로 남김) |
| 도구 전체 orphan ID | 14 | **6** |
| 도구 전체 인용 | 58 | **17** |

---

## 0. 반증 — cp949 수정(`311718f`)은 동작합니다

`PYTHONIOENCODING` **없이** 이 콘솔에서:

```
$ python tools/docs/check_req_citations.py
[check_req_citations] no new orphans (6 known orphan ids, 17 citations)
EXIT=0
```

이전에는 같은 명령이 `UnicodeEncodeError: 'cp949' codec` 으로 죽었습니다. **재현되던 것이 사라졌습니다.**

---

## 1. 카드가 지목한 셋 — 전부 예상과 다른 이유로 해소

### `007` 의 INFO 알림 — **어느 인용도 알림을 서술하지 않습니다**

`007` 인용 4곳(`preprocess_api.h:630`, `temp_compensate.cpp:17`·`:32`,
`test_temp_nonlinearity_binning.cpp:62`)은 **전부 "NaN → 25.0C fallback"** 만 말합니다.
알림을 서술하는 자리는 **0건**입니다. 그래서 넷 다 `081` 로 옮길 수 있었습니다.

### `008` 의 플래그 주체 — **인용이 애초에 플래그를 말한 적이 없습니다**

카드는 *"`008` 인용이 `temp_compensate.cpp` 안에 있으면 그 함수는 플래그를 안 세우므로 `082` 가 맞지
않을 수 있다"* 고 경고했습니다. 실제로는 더 단순하고 더 나쁩니다 — **`008` 인용 7곳이 전부
플래그가 아니라 온도 범위 가드를 서술합니다.**

```
preprocess_api.h:631   REQ-P1A-008: Temp out of [-20, +60] range -> XPE_ERR_INVALID_INPUT
temp_compensate.cpp:35 REQ-P1A-008: temp out of [-20, +60] range -> XPE_ERR_INVALID_INPUT
golden:495             REQ-P1A-008: Out-of-range temperature returns XPE_ERR_INVALID_INPUT
test_temp_...:70       REQ-P1A-008: temperature out of [-20, +60] returns XPE_ERR_INVALID_INPUT
```

그 내용은 **옛 `006`** 입니다(*"IF detectorTempC is outside [-20.0, +60.0] … return INVALID_INPUT"*).
즉 **번호가 두 칸 밀린 채 처음부터 틀려 있었습니다.** → `081`(Temperature Input Guard)이 정확히 덮습니다.

그래서 **`082`(파이프라인이 플래그를 세운다)를 가리키는 인용은 `modules/preprocess` 에 없습니다.**
카드가 걱정한 "함수 안에서 `082` 를 인용" 은 발생하지 않았습니다.

곁가지: `golden:472` 의 `REQ-P1A-006` 인용은 *"Above 25°C … corrected < raw"* 로 **실행 동작**을
말합니다 — 옛 `006`(범위 가드)이 아니라 옛 `005` → 현재 **`080`**. 역시 밀려 있었습니다.

### `023` — 같은 형태입니다

옛 `023` 은 *"비닝 성공 후 `XPE_FLAG_BINNING_CORRECTED` 설정"* 인데, 인용 6곳 중 내용을 가진 셋은:

| 자리 | 서술 | 실제 |
|---|---|---|
| `preprocess_api.h:667` | "Per-mode correction profile" | 옛 `020` → **`090`** |
| `binning_correct.cpp:29` | "per-mode correction profile" | 옛 `020` → **`090`** |
| `golden:550` | "unknown mode returns `XPE_ERR_CONFIG_INVALID`" | 옛 `022` → **`091`** |

**플래그를 말하는 인용은 하나도 없습니다.** 카드의 "`091` 이냐 `096` 이냐" 는 물음은
**둘 다 아니고 `090`·`091` 로 갈린다** 가 답입니다.

---

## 2. 덮지 못하는 자리 — **이것이 이 카드의 산출입니다**

### (a) `REQ-P1A-034` 인용 4곳은 `ghost_reset` 을 말하는데, **덮는 요구가 없습니다**

| 자리 | 서술 |
|---|---|
| `preprocess_api.h:601` | "Clear accumulated frame history" |
| `ghost_correct.cpp:251` | "clear accumulated frame history" |
| `test_ghost_correct.cpp:65` | "`xpe_ghost_reset` clears history" |
| `golden:386` | "After `reset()`, next frame uses zero history" |

옛 `034` 는 *"고스트 보정 성공 후 플래그 설정"* 입니다. 이 인용들이 말하는 것은 옛 **`032`**
(*"`xpe_ghost_reset` 호출 시 핸들을 파괴하지 않고 프레임 이력을 비운다"*)입니다 — **또 밀려 있었습니다.**

현재 요구 중 `xpe_ghost_reset` 을 언급하는 것은 **`086`(무효 핸들 가드) 하나**이고, 그것은
"이력을 비운다" 를 말하지 않습니다. **`085`·`087` 도 아닙니다.**

→ **4곳 모두 손대지 않았습니다.** `096`(파이프라인 플래그)으로 옮기면 인용 내용과 정반대가 됩니다.

### (b) `test_req_p1a_066.cpp` — **네 시험 중 `066` 도 `086` 도 검증하는 것이 없습니다**

카드 §3 의 결정표에서 **"섞여 있다 → 건별로 갈라 보고"** 에 해당합니다. 다만 섞인 방향이
예상과 다릅니다 — `086` 을 검증하는 것이 **0건**입니다.

| | 단언하는 것 | 실제로 해당하는 요구 |
|---|---|---|
| `T1` | `xpe_gain_correct` 가 `UINT32_MAX×UINT32_MAX` 를 **할당 전에** 거부(`n > SIZE_MAX/sizeof(float)`) | **없음.** `005`(Input Validation)는 NULL·0 치수만 말하고 **곱셈 오버플로를 말하지 않습니다** |
| `T2` | 디렉터리 경로로 `xpe_calib_load_offset` → `IO_FAILED` | `014` 의 오류 경로 |
| `T3` | sha256 을 0 으로 만든 xcal → `CONFIG_INVALID` | `014`("validate SHA-256 integrity")의 오류 경로 |
| `T4` | **각자 자기 핸들을 가진** 4스레드가 동시 실행, 무충돌 | `003`(Thread Safety, 독립 버퍼 재진입성) |

옛 `066` 은 *"핸들을 스레드 간 **공유 금지**"* 인데, **`T4` 는 공유하지 않는 경우 — 즉 허용되는
쪽 — 을 검증합니다.** 파일 주석도 *"The ghost handle is NOT thread-safe for sharing"* 이라 적고
그 **반대 사례**를 돌립니다. 금지된 동작을 검증하는 시험은 이 파일에 없습니다.

→ **인용도 파일명도 바꾸지 않았습니다.** 카드가 인용한 `#207` 원칙대로 이름과 내용은 같이 고쳐야
하는데, 고칠 방향 자체가 요구 신설(특히 `T1` 의 오버플로 가드)에 달려 있습니다. **리더 판단 필요.**

### (c) 비선형 보정에 **요구가 없습니다**

옛 `012`~`015`(`xpe_nonlinearity_correct` 실행·선형 응답·플래그·미지 모드)가 `bc22093` 에서
사라졌고 **대체 요구가 없습니다.** 구현은 있고 `#196`·`#199` 에서 두 번 손댔습니다.
`test_temp_nonlinearity_binning.cpp` 가 이 경로를 돌리므로 그 파일 머리에 사실을 적었습니다 —
이웃 번호로 옮기지 않았습니다.

### (d) 옛 `043`·`046` 이 `095`~`099` 에 없습니다

`041 to 047` 범위를 `095 to 099` 로 옮겼는데, 그 범위가 덮던 것 중 둘이 새 요구에 없습니다:

- 옛 `043`: uint16 → float32 **도메인 전환이 stage 2(게인)에서 일어난다**
- 옛 `046`: 각 단계가 **실행 전에 캘리브레이션 가용성을 확인**한다

### (e) 옛 `045` 의 WARNING 알림 — **구현에 없습니다(098 이 맞습니다)**

옛 `045` 는 핸들이 없으면 스킵하고 *"WARNING 알림을 게시"* 하라고 했습니다.
`pipeline.cpp:280` 은 `if (!cfg.bypassGhost && ghostHandle)` 로 **조용히 건너뜁니다** — 알림 0건.
`098` 이 알림을 빼고 쓴 것이 구현과 맞습니다. `007` 의 INFO 알림과 같은 처리입니다.

### (f) **SPEC 안의 모순** — `§5.6` 이 신설 요구의 대상 함수들을 "제외" 로 적고 있습니다

`spec.md:731` **"5.6 Excluded Functions (Phase 2+)"**:

| 함수 | 사유 | Target SPEC |
|---|---|---|
| `xpe_ghost_create/correct/reset/destroy` | Stateful handle architecture | SPEC-XPE-P1B |
| `xpe_temp_compensate` | MCU migration design needed | SPEC-XPE-P1C |
| `xpe_nonlinearity_correct` | Separate SWU | SPEC-XPE-P1D |
| `xpe_binning_correct` | Fluoro/CBCT only | SPEC-XPE-P1E |

**`§4.3b` 가 바로 이 함수들에 `080`~`091` 을 세웠습니다.** 같은 문서가 한쪽에서는 제외라 하고
다른 쪽에서는 요구를 답니다. `.moai/specs/` 는 리더 소유라 **손대지 않았습니다.**

---

## 3. 고친 인용 — **35곳 / 10파일**

| 옛 → 새 | 곳 | 비고 |
|---|---|---|
| `006` → `080` | 1 | 인용 내용이 실행 동작(옛 `005`)이었음 |
| `007` → `081` | 4 | 알림 서술 0건이라 전부 이동 가능 |
| `008` → `081` | 5 | 인용 내용이 범위 가드(옛 `006`)였음 |
| `005 to 008` → `080 to 082` | 2 | 서브시스템 범위 |
| `023` → `090` | 2 / `091` | 1 | 내용별로 갈라 배치 |
| `020 to 023` → `090, 091` | 2 | |
| `029` → `085` | 5 | 인용 내용이 옛 `029` 와 일치 — 이 카드에서 **유일하게 안 밀린 축** |
| `029 to 034` → `085 to 087` | 6 | |
| `041 to 047` → `095 to 099` | 5 | |
| `REQ-P1A-026/027` → `SRS-CALIB-FUNC-026/027` | 2 | 접두사만, 번호 그대로(`#203`) |

---

## 4. 반증 — 도구 초록은 근거가 아닙니다

`A-149` 와 같습니다. 이번 카드에서 도구가 못 보는 것이 **여섯 종**입니다: `034`(reset),
`066`(파일 전체), 비선형, 옛 `043`·`046`, 옛 `045` 알림, 그리고 `§5.6` 모순. 이 중 도구가
신호를 주는 것은 앞의 둘(orphan 으로 남음)뿐이고, **나머지 넷은 인용이 없거나 이미 초록이라
영원히 보이지 않습니다.**

근거는 옛 원문(`d6bde7d`)·현재 정의·**인용 자리의 서술** 셋을 나란히 읽은 것입니다.
특히 이번에는 **인용 자리의 서술**이 결정적이었습니다 — 세 축(`006/007/008`, `023`, `034`)에서
인용이 옛 번호와도 어긋나 있었고, 그것은 옛 원문만 봐서는 알 수 없습니다.

---

## 검증

| 항목 | 결과 |
|---|---|
| 빌드 | `BUILD_EXIT=0` |
| 전체 ctest | **757 / 757** (`CTEST_EXIT=0`) |
| `check_req_citations.py` (PYTHONIOENCODING 없이) | `no new orphans` — 14→6 ID, 58→17 인용 |
| `modules/preprocess` orphan | 10 → **2** (`034` 4곳, `066` 6곳 — 의도적) |

## 미검증 / 잔여 위험

- **`T2`·`T3` 를 `014` 의 오류 경로로 본 것은 읽어서 판단한 것**입니다. `014` 본문은 "SHA-256
  무결성 검증" 을 말하지만 실패 시 어느 코드를 내는지는 명시하지 않습니다 — `CONFIG_INVALID` 가
  맞는지는 요구 쪽에서 정할 일입니다. 그래서 인용을 옮기지 않았습니다.
- 다른 모듈은 보지 않았습니다.
- baseline 은 갱신하지 않았습니다(리더가 병합 뒤에 하기로).
- `§5.6` 모순은 **읽어서 발견한 것**이고, 그 표가 아직 유효한 결정인지는 확인하지 못했습니다 —
  오래된 표가 남은 것일 수도 있습니다.

🗿 MoAI

---

# 정정 (2026-09-28) — §2(c) "비선형 보정에 요구가 없습니다" 는 **틀렸습니다**

요구는 **있습니다**: `SRS-CALIB-FUNC-006` / `-006-EXT` (`SRS-CALIB-001`).
구현이 이미 인용하고 있습니다 — `nonlinearity_correct.cpp:4`·`:37`.
독립 확인: `docs/calibration/` 의 네 문서가 같은 ID 로 이 함수를 매핑합니다.

**오류의 형태는 부재 단언의 범위입니다.** `REQ-P1A-` 정의 한 표만 보고
"요구가 없다" 를 **전 계열에 대해** 단정했습니다. 검색이 눈먼 것이 아니라
**검색한 범위를 결론의 범위로 잘못 옮긴 것**입니다.

더 나쁜 것은 답이 **옆 파일에 이미 있었다**는 점입니다. `nonlinearity_correct.cpp:6-8` 의
머리말이 *"The REQ-P1A-012..015 citation that used to sit here named four requirements about
defect correction and calibration loading"* 라고 적고 **옳은 ID 를 바로 위에 인용**합니다.
`#196`·`#199` 에서 제가 손댄 파일입니다.

**도구도 같은 범위입니다.** `check_req_citations.py` 의 orphan 목록은
*"REQ-P1A 요구가 없다"* 이지 *"요구가 없다"* 가 아닙니다. 리더 확인으로 이 함정에
두 번 물렸습니다 — `xpe_calib_get_quality_meta`(`SRS-CALIB-FUNC-033`, `#140`)와 이번 건.

조치: `test_temp_nonlinearity_binning.cpp` 머리말의 틀린 문장을 **지우지 않고 정정으로
바꿨습니다**(`718ba06`) — 무엇을 왜 틀렸는지 남겨야 다음 사람이 같은 범위 착각을 반복하지
않습니다. 옛 `REQ-P1A-012~015` 가 대체 없이 사라진 것도 **결함이 아니라 정상**입니다.

나머지 판정((a)·(b)·(d)·(e)·(f))은 리더가 확인했고 변경 없습니다.
`§5.6` 표는 리더가 죽은 계획임을 확인해 무효 표기했습니다(`c69babc`) — 지우지 않은 이유는
같습니다.

검증: `BUILD_EXIT=0`, 전체 ctest **757/757**.

🗿 MoAI
