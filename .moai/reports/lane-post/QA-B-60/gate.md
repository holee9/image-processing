# QA-B-60 게이트 보고서 — 조용히 무시되는 설정 키를 말하게 한다

**카드**: QA-B-60 (#145 #142)
**레인**: Lane B (`xpe-post`, `dev/postprocess`)
**증거 경로**: `.moai/reports/lane-post/QA-B-60/`
**커밋 1건**: `531a163`
**선행**: `git merge origin/main` 완료

---

## 1. config 진입점 전수

post 소유 모듈에서 JSON config 를 받는 진입점은 **9개**다(`configJsonOrNull` 인자 기준).

| # | 진입점 | 파서 | 미지 키를 주면 | 분류 |
|---|---|---|---|---|
| 1 | `xpe_gsvg_init` | 수제 불리언 스캔(`gsvg.cpp:56`) | 변화 없음·`XPE_OK`·무언 | **조용히 무시** → §2 에서 경고 배선 |
| 2 | `xpe_ai_init` | nlohmann(`ai.cpp:186~`) | 키가 조건문에 안 걸림·`XPE_OK`·무언 | **조용히 무시** → §2 에서 경고 배선 |
| 3 | `xpe_enhance_advanced_init` | nlohmann, **구문 검사만** | `XPE_OK` (문법 오류면 `XPE_ERR_CONFIG_INVALID`) | **조용히 무시 — §3** |
| 4 | `xpe_multiscale_process` | `parse_mfp_config`(nlohmann) | 기본값 사용·무언 | **조용히 무시 — §4 로 미배선** |
| 5 | `xpe_fractional_process` | `parse_fractional_config` | 같음 | 같음 |
| 6 | `xpe_detect_collimation` | `parse_collimation_config` | 같음 | 같음 |
| 7 | `xpe_stitch_images` | (stub 경로) | 추론 전 반환 | **측정 불가**(B-59 §3.3 구분) |
| 8 | `xpe_bone_suppress` | 같음 | 같음 | 같음 |
| 9 | `xpe_dl_denoise` | 같음 | 같음 | 같음 |

**오류를 내는 것은 없다.** 문법이 깨진 JSON 만 `xpe_enhance_advanced_init` 이 거절한다 —
**이름을 모르는 키는 어디서도 거절되지 않는다.**

(참고: `xpe_init` 은 `modules/common` — Lane A 소유라 목록만 적고 손대지 않았다.)

---

## 2. 경고 배선 (`531a163`)

**반환 코드는 바꾸지 않았다.** 거절은 동작 변경이고 #145 의 결정 사항이다. 채널은
`xpe_alert_push(msg, XPE_ALERT_WARNING)` — SRS-ALERT 가 정의하고 호스트가 이미 폴링하는 경로다.

**메시지에 키 이름을 넣는다.** "알 수 없는 키가 있다" 로는 `grid_supression` 을 찾을 수 없다.

### 2.1 gsvg — 스캐너, 의존성 없이

이 모듈에는 JSON 라이브러리가 없다. 경고 하나를 위해 의존성을 사는 대신 중괄호 깊이와
따옴표를 추적하는 스캐너를 썼고, **모든 모호함을 침묵 쪽으로 해소**한다:

- 이름은 **깊이 1** 에서 `:` 가 따라올 때만 키로 센다 → 중첩 객체 안의 키는 보고하지 않는다
- 문자열 **값**은 키로 세지 않는다
- `NULL` config 는 스캔 자체를 하지 않는다(기본값 사용은 정상 경로)

근거: 놓친 키는 **있었으면 좋았을 경고 하나**를 잃지만, 정상 config 의 헛경고는 읽는 사람을
경고 무시에 길들이고 그 대가는 **다음 진짜 경고**다.

### 2.2 ai — nlohmann, 타입까지

최상위 키를 직접 순회한다. 여기엔 gsvg 에 없는 경우가 하나 더 있다 — 이 파서는 **값의 타입이
맞아야** 키를 소비하므로 `"timeout_ms": "500"` 은 조용히 버려진다. **한눈에 맞아 보이는
실수**라 별도 문구로 보고한다:

```
ai config key 'timeout_ms' has an unexpected type and was ignored
ai config key 'gpu_memory_limit_mb' is not read by this module and has no effect
```

---

## 3. 전수에서 나온 것 — `xpe_enhance_advanced_init` 은 config 를 버린다

```cpp
nlohmann::json config = nlohmann::json::parse(configJsonOrNull);
// TODO: Store configuration parameters for use in processing functions
// For now, just validate JSON format
```

**구문만 검사하고 아무것도 저장하지 않는다.** 그래서 이 진입점에서는 **모든 키가 정의상
미소비**다.

경고를 달지 **않았다**. 달면 정상 호출까지 전부 울린다 — 호출자가 같은 JSON 을 init 과
process 에 함께 넘기는 흔한 형태에서, init 은 "이 키는 효과 없음" 을 외치고 process 는 그
키를 정상 소비한다. **그것이 카드가 경계한 경고 피로 그 자체다.**

**판정 요청**: 이 진입점의 `configJsonOrNull` 은 (a) TODO 를 구현해 실제로 소비하거나,
(b) 인자를 문서에서 "현재 미사용" 으로 명시하거나 — 둘 중 하나가 필요하다.
경고는 그 결정 뒤에 얹는 것이 맞다.

---

## 4. 배선하지 않은 축과 그 이유

| 축 | 이유 |
|---|---|
| `xpe_multiscale_process` · `xpe_fractional_process` · `xpe_detect_collimation` | **프레임마다 호출되는 경로**다. 미지 키 하나가 프레임당 경고 하나가 되면 큐가 그것만으로 찬다 — 진짜 경고를 밀어내는 형태의 경고 피로다. init 처럼 **한 번 불리는 지점**과 성격이 다르다 |
| ai 추론 3종 | stub 이 추론 전에 반환한다(B-59 §3.3) — "도달 못 함" 이지 "무반응" 이 아니다 |
| `xpe_init`(common) | **Lane A 소유** |

---

## 5. RED → GREEN

| 케이스 | gsvg | ai |
|---|---|---|
| `NULL` config | **무경고** | **무경고** |
| 빈 객체 `{}` | **무경고** | **무경고** |
| 전 키 유효 | **무경고** | **무경고** |
| 미지 키 | 이름 보고 | 이름 보고 |
| 알려진 키의 오타 | `grid_supression` 보고 | `timout_ms` 보고 |
| 알려진 키·틀린 타입 | — | `timeout_ms` **타입 문구로** 보고 |
| 문자열 값 오인 | 보고 안 함 | (파서가 구조를 알아 해당 없음) |
| 중첩 키 | `inner_key` 보고 안 함, `nested` 는 보고 | (해당 없음) |
| 반환 코드 | `XPE_OK` 불변 | `XPE_OK` 불변 |

`_green.log`(gsvg 8/8) · `_green_ai.log`(ai 7/7), 둘 다 **BUILD=0**.

**정상 config 의 무경고를 단언으로 박은 것이 이 카드의 절반이다.** 경고가 평상시에 뜨면
읽는 사람이 경고를 끄고, 그 순간 이 기능은 순손실이 된다.

---

## 6. 반증 (`_falsify.log`)

gsvg 스캐너의 `depth == 1` 을 `depth >= 1` 로 **약화**(삭제 아님):

```
===BUILD=0===
alert: gsvg config key 'nested' is not read by this module and has no effect
alert: gsvg config key 'inner_key' is not read by this module and has no effect
[  FAILED  ] GsvgConfigWarning.NestedKeysAreNotReported
===GSVG_EXIT=1===
```

**중첩 키가 최상위로 보고되며 침묵 쪽 단언이 실패한다** — 헛경고를 막는 단언이 실제로
일하고 있다는 뜻이다. 다른 7건은 통과한다(경고를 내는 쪽 단언은 이 약화로 깨지지 않는다).
반증 뒤 원복했다.

---

## 7. Baseline 귀속 (Baseline-attribution)

| 항목 | 값 | 근거 |
|---|---|---|
| config 진입점 | **9개** 전수 | §1 |
| 분류 | 조용히 무시 6 · 측정 불가 3 · 오류 0 | §1 |
| 경고 배선 | 2개 진입점(`xpe_gsvg_init` · `xpe_ai_init`) | `531a163` |
| GREEN | BUILD=0, gsvg 8/8 · ai 7/7 | `_green.log` / `_green_ai.log` |
| 반증 | BUILD=0, 침묵 단언 실패 | `_falsify.log` |
| 이전 ctest | 503 / 214 / 177 | QA-B-59 `_verify.log` |
| 현재 ctest | **512 / 222 / 177** (신규 15건) | `_verify.log` |
| 빌드 경고 | 0 | `_verify.log` |

---

## 8. 문서 목록 추가분 (leader 처리 — 수정하지 않음)

B-59 가 준 목록(`api-spec.md:180`, `api-spec §12.7`, `gsvg/README.md:601-602, 708`)에 더해:

1. **`xpe_enhance_advanced_init` 의 `configJsonOrNull` 이 무엇을 받는지 어디에도 없다.**
   실제로는 **아무것도 소비하지 않는다**(§3). 문서가 키를 말한다면 그 목록이 통째로
   유령이고, 말하지 않는다면 인자의 존재 이유가 문서에 없다.
2. **`xpe_ai_init` 이 읽는 키 4개**(`execution_provider` · `timeout_ms` ·
   `confidence_threshold` · `fallback_mode`)**가 api-spec 에 없다.** 코드가 문서보다 넓은
   드문 방향이다 — 지금까지 찾은 것은 전부 반대였다.
3. **`xpe_gsvg_init` 이 읽는 키 2개**(`vignette_correction` · `grid_suppression`)**도
   api-spec 에 없다.** 문서는 읽지 않는 키만 말하고 읽는 키는 말하지 않는다.

---

## 9. 미검증 (Gaps)

- **per-call config 경로 3개는 배선하지 않았다**(§4). 미지 키가 거기서도 조용히 무시되는
  것은 §1 에서 확인했지만, 경고는 달지 않았다 — 이유는 적었다.
- **`xpe_enhance_advanced_init` 도 배선하지 않았다**(§3) — 판정이 선행한다.
- **gsvg 스캐너는 JSON 파서가 아니다.** 주석 없는 순수 JSON 을 전제하며, 이스케이프와
  중첩은 다루지만 **문법 오류가 있는 입력에서의 동작은 정의하지 않았다** — 그런 입력은
  애초에 `json_get_bool` 도 제대로 읽지 못한다.
- **알림 큐 용량을 넘기는 경우를 재지 않았다.** 키가 아주 많은 config 는 큐를 채울 수 있다.
- **경고가 실제로 호스트에 보이는지는 이 레인에서 확인할 수 없다** — GUI 가 알림 큐를
  폴링하는지는 Lane C 영역이다.

---

## 10. 잔여 위험 (Residual-risk)

- **경고는 무시될 수 있다.** 호출자가 알림 큐를 읽지 않으면 침묵과 같다. 반환 코드를 바꾸지
  않기로 한 결정의 대가이고, #145 가 그 결정을 다룬다.
- **"경고가 없다" 가 "설정이 적용됐다" 는 뜻은 아니다.** 이름과 타입이 맞아도 값이
  무시되는 경로(§3 의 enhance_advanced 가 그 예)는 이 경고가 잡지 못한다.
- **키 이름을 메시지에 넣는 것은 입력을 로그에 싣는 것이다.** 여기서는 config 키 이름이라
  민감 정보가 아니지만, 값이 아니라 **키만** 싣는다는 선을 유지해야 한다.
- 커밋은 push 전까지 미푸시 유일본이다.

---

## 부록 — 증거 파일

| 파일 | 내용 |
|---|---|
| `_env.bat` / `_verify.bat` | 환경 + 세 프리셋 재빌드·전체 ctest |
| `_b60g.bat` / `_green.log` | gsvg 8건 |
| `_b60ai.bat` / `_green_ai.log` | ai 7건 |
| `_falsify.log` | 깊이 검사 약화 — BUILD=0, 침묵 단언 실패 |
| `_verify.log` | 최종 512 / 222 / 177, 경고 0 |
