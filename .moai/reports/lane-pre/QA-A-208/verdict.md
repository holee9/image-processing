# QA-A-208 — Codex #34 보류 2건: 게인 bypass 뒤 float 단계의 형식 전환, XCal 품질 필드의 최상위 키

기준 커밋 `f2ca59d7`(QA-A-202d) 위. 감사 원문은 `evidence/00_codex34_audit_original.md`. 증거는 `evidence/` (번호 순). 이슈 #234, #233.

**순서 보고.** 리더가 "202d 가 길면 208 을 먼저 해도 됨" 이라 했다. 202d 를 먼저 끝냈고(`f2ca59d7`), 이어서 208 을 했다. 이 사이에 Codex #38 이 202d 를 보류해 새 카드 QA-A-202e 가 생겼다 — 이 카드는 건드리지 않았다(순서: 208 → 202e → 204 3/3).

## 1. 주장

| # | 주장 | 근거 |
|---|---|---|
| 1 | 게인 bypass 는 "게인 = 1" 이다: float32 를 요구하는 단계(비닝>1, 결함, 고스트)가 뒤에 오면 그 앞에서 uint16 → float32 변환을 **명시적으로** 하고, 각 float 단계에 들어가는 버퍼의 형식·바이트 수를 확인한다(어긋나면 `PROCESSING_FAILED`). 205b 의 `final_result_is_float` 예측이 실제 단계 실행과 일치한다. | 수정 전 재현 `02`·`03`, 초록 `06`, 반증 `arm_noconvert*` |
| 2 | XCal 게인의 품질 필드는 설정 객체의 **최상위 키**만 본다(문자열·중첩 객체·배열을 건너뛰며 구조적으로 걸음). 같은 최상위 키가 두 번이면 `XPE_ERR_CONFIG_INVALID`. | 수정 전 `04`, 초록 `07`, 반증 `arm_strstr_back*`, `arm_dup_allowed*`, `arm_escape_ignored*` |
| 3 | (리더 요청) 파이프라인 설정 JSON(`xpe_json_get_string`)은 바꾸지 않았고, 같은 한계(첫 출현을 읽음)가 있음을 실측으로 확인했다. | `09_pipeline_config_first_occurrence_probe.txt` |

## 2. 수정 전 재현 (카드가 요구한 "실측 먼저")

감시 영역 할당기를 만들었다: `VirtualAlloc` 으로 블록의 끝을 커밋된 페이지의 끝에 맞추고 바로 뒤에 `PAGE_NOACCESS` 페이지를 둔다. 블록 뒤를 읽거나 쓰면 접근 위반이고, 호출을 SEH 로 감싸 잡아 그 구성의 실패로 보고한다. (기존 카나리아 방식은 **쓰인** 바이트만 알아채므로 초과 읽기를 못 본다.) Windows 전용이다 — 다른 플랫폼에서는 시험이 건너뜀 처리된다(`.github/workflows/` 에서 ubuntu/linux 를 쓰는 파일은 `label-sync.yml` 하나뿐임을 확인했다).

- **Codex 가 적은 재현** `bypassGain + 비닝 2 + 온도 on`(N 화소, 용량 N×4): 수정 전 **접근 위반**(`02_red_combos.txt`, `AGainBypassFollowedByBinning…` 빨강). 비닝의 `memcpy(N×4)` 가 게인-bypass 가 넘긴 N×2 단계 버퍼를 넘어 읽었다. 가설이 아니라 재현됐다.
- **설정 조합 전수 시험**(온도·오프셋·게인·비닝{bypass,1,2}·결함·고스트 = 96 구성, 같은 입력): 수정 전 **40 구성 실패**(`03_red_summary.txt`):

| 실패 방식 | 구성 수 |
|---|---|
| 단계 버퍼 밖 읽기/쓰기(접근 위반) | 18 |
| 결함 단계가 uint16 프레임을 거절(`XPE_ERR_UNSUPPORTED_FORMAT`, rc −7) | 16 |
| 완주했지만 "게인 맵 = 전부 1" 로 돌린 결과와 다름(앞 단계가 모두 bypass 라 호출자 버퍼의 상위 절반을 float 로 읽음) | 6 |

  나머지 56 구성은 수정 전에도 통과했다 — 게인이 켜진 48 구성과, 게인이 꺼졌지만 float 단계가 없어 `uint16` 으로 끝나는 8 구성.

## 3. 무엇을 바꿨나

- **`pipeline.cpp`** — 게인 단계 else 분기: 게인이 bypass 이고 float 단계가 뒤에 오며(`final_result_is_float`) 입력이 uint16 이면 `stage4Data`(float N 개)로 `static_cast<float>` 변환하고 형식 필드(float32, 32비트)를 채운다. 그 외에는 예전처럼 `stage4 = stage3`. `stage_input_is()` 로 비닝·결함·고스트 직전에 입력 버퍼의 형식(float32)·크기(N×4)를 확인하고 아니면 `PROCESSING_FAILED`. 마지막 복사 직전에는 최종 단계의 형식도 예측과 맞는지 본다.
- **`helpers.cpp` / 내부 헤더** — `xpe_json_top_level_scalar()`(+ `XpeJsonTop`: 없음/스칼라/비스칼라/중복/깨짐). 객체를 멤버 단위로 걸으며 문자열(이스케이프 포함)·중첩 객체·배열을 건너뛴다. 설정이 비어 있으면 "없음", 객체가 아니거나 도중에 깨지면 "깨짐".
- **`xpe_calib_mode.cpp`** — 품질 네 필드가 그 함수를 쓴다: 없음 → 건너뜀, 스칼라 → 엄격 변환(빈 값은 거부), 비스칼라·중복·깨짐 → `CONFIG_INVALID`.
- 헤더 계약: 게인 bypass 의미(`preprocess_api.h`), 품질 필드 계약 문장은 아래 5절.

## 4. 시험

- `AGainBypassFollowedByBinningDoesNotReadPastTheStageBuffer` — Codex 의 재현 구성.
- `EveryConfigurationReadsOnlyWhatItOwnsAndEndsInTheFormatItIsPredictedToEndIn` — 96 구성 × (접근 위반 없음, rc OK, 최종 형식 == 시험이 **독립적으로** 적은 예측(`gain || binning==2 || defect || ghost`), 최종 프레임 뒤 바이트 불변, 게인 bypass 결과 == 게인 맵 1.0 으로 돌린 결과 — float 로 끝나는 구성 중 게인이 꺼진 40 구성에서 바이트 단위로 같음). 실패한 구성은 첫 실패에서 멈추지 않고 전부 나열한다. 통과 시 `[combo] 96 configurations; 88 end in float32, 8 in uint16; 40 compared with a gain map of ones`.
- `AQualityFieldIsTakenFromTheTopLevelOfTheConfigOnly` — 품질 네 필드 각각 × 11 행: 중첩에만 있음(유효/불량) → 없음으로 처리, 중첩 유효 + 최상위 불량(앞/뒤) → 거부, 중첩 배열 안에만 → 없음, 최상위 중복(유효·유효 / 유효·불량 / 불량·유효) → 거부, 문자열 값 끝의 `\"키` 가 키로 오인되지 않음(뒤에 진짜 키가 있으면 그것을 읽음), 문자열 안의 중괄호·따옴표는 세지 않음. 읽힌 값이 7 인 것은 저장소의 0.5 / 1 / 3 / 2 와 모두 달라 읽혔음이 변화로 보이게 하려는 것이다.

## 5. 계약 문장 (리더가 api-spec 에 옮김)

> **게인 bypass.** 파이프라인에서 게인 단계를 bypass 하는 것은 "게인 = 1" 이다. 뒤에 float32 를 요구하는 단계(binningMode>1 인 비닝, 결함, 핸들이 있는 고스트)가 하나라도 오면 프레임은 그 앞에서 uint16 → float32 로 명시적으로 변환되고, 결과는 게인 맵이 전부 1.0 일 때와 비트 단위로 같다. 그런 단계가 없으면 프레임은 uint16 으로 남는다. 최종 형식은 게인·비닝(>1)·결함·고스트(핸들 있음) 중 하나라도 돌면 float32, 아니면 uint16 이다(205b 의 용량 계약과 같은 판정).

> **XCal 게인의 품질 필드(수정).** `fit_r_squared`, `polynomial_degree`, `actual_dose_levels`, `calibration_mode` 는 설정 JSON 객체의 **최상위** 키만 본다. 중첩 객체나 배열 안의 같은 이름, 문자열 값 안에 들어 있는 이름은 키가 아니다. 최상위에 없으면 "주어지지 않음", 있으면 숫자여야 하고(205b 의 규칙), 같은 최상위 키가 두 번 나오면 `XPE_ERR_CONFIG_INVALID` 이다. 설정이 JSON 객체가 아니거나 닫히지 않는 등 구조가 깨져 있으면(빈 텍스트는 키가 없는 설정이다) 마찬가지로 `XPE_ERR_CONFIG_INVALID` 이며, 어느 경우에도 게인 맵과 품질 메타데이터는 호출 전 그대로 남는다.

**호환성 메모.** 구조가 깨진 설정을 품질 필드 조회가 거부하는 것은 이번에 새로 생긴 동작이다(전에는 `strstr` 이 깨진 텍스트 속에서도 키를 찾아냈다). 제품 생성기는 항상 잘 짜인 객체를 쓰고, 이번 전체 시험(생성기 산출물 포함)에서 새로 거부된 파일은 없다. 현장 구형 XCal 은 저장소 밖이라 점검하지 못했다.

## 6. 파이프라인 설정 JSON 의 같은 한계 (리더가 따로 정함)

`xpe_json_get_string` — 파이프라인 설정(`bypass*`, `detectorTempC`, `binningMode` 등)을 읽는 함수 — 은 이번에 **바꾸지 않았다**. 첫 출현을 읽는다는 것을 실측으로 확인했다(`09_pipeline_config_first_occurrence_probe.txt`; DLL 을 `ctypes` 로 직접 불러 대조군을 짝지었다): 모두 bypass 로 두고 `bypassGain` 만 최상위에서 `false` 로 주면 게인 단계가 돌아 교정 미적재 오류(rc −16) 가 나고(대조 2), 최상위 `false` 앞에 `{"nested":{"bypassGain":true}}` 를 두면 중첩 값이 읽혀 게인이 bypass 되어 rc 0 이 된다(프로브). 이 한계는 잘못된 설정이 조용히 다른 의미로 읽히는 쪽이다. 고치려면 같은 `xpe_json_top_level_scalar` 를 쓰면 되지만, "빈 값 = 키 없음"(202b)과 중복 키·깨진 설정을 어떻게 다룰지는 정책이라 리더 결정을 기다린다.

## 7. 실측 (이 트리, 이 실행)

| 항목 | 관측 |
|---|---|
| 빌드 | 전체 타깃 `BUILD_EXIT=0` (`21_build_final.txt`) |
| preprocess DLL 시험 | 777 실행 / 769 통과 / 8 건너뜀(원래 건너뛰던 8건), 섞기도 동일 (`24`, `25`) |
| preprocess OOM exe | 35 통과 (`26_pre_oom.txt`; 직전 33) |
| common OOM exe / common 기존 | 12 / 69 통과 (변화 없음) |
| `ctest -N` | 933 (직전 930, 새 시험 3건) |
| 수출 이름 | preprocess 48, common 16 불변 (`29_exports_pre_diff.txt`) |
| 헤더 문서 / 프리셋 | 0 findings / 12 of 12 |

## 8. 반증 (한 번에 하나, 전체 빌드 후 OOM exe 와 DLL 시험 실행, 복원 뒤 `cmp` 동일)

| 팔 | 손상 | 빨개진 시험 |
|---|---|---|
| noconvert | 변환 분기 제거(방어 검사는 그대로) | 조합 전수 시험(40 구성 실패), Codex 재현 시험 |
| noconvert_nodefense | 변환 분기와 세 방어 검사 모두 제거 | 같은 두 시험 |
| strstr_back | 품질 필드 조회를 205b 의 `xpe_json_find_scalar`(첫 출현)로 되돌림 | `AQualityFieldIsTakenFromTheTopLevel…` |
| dup_allowed | 최상위 중복 키를 거부하지 않음 | 같은 시험 |
| escape_ignored | 문자열 건너뛰기에서 백슬래시 이스케이프를 무시 | 같은 시험 |

**방어 검사가 하는 일**(`08_defense_effect.txt`): 변환만 빼고 방어를 남긴 팔은 40 구성이 모두 `PROCESSING_FAILED`(rc −3)로 멈추고 접근 위반은 **0** 이다. 방어까지 뺀 팔은 18 구성이 접근 위반, 16 이 rc −7, 6 이 값 불일치로 수정 전과 같다. 즉 변환이 틀리거나 빠져도 읽기 초과가 아니라 오류로 끝난다.

## 9. 미검증 (Gaps)

- **게인 bypass 경로의 할당 실패 스윕을 더하지 않았다.** 새 변환은 다른 단계와 같은 모양의 `vector` 할당 하나이고 파이프라인 가드가 `bad_alloc` 을 `OUT_OF_MEMORY` 로 바꾸지만, 이 구성으로 K 번째 할당 실패를 주입하지는 않았다.
- **감시 영역 할당기는 Windows 전용**이고, 블록 끝을 16바이트 경계로 맞춰 둔 탓에 블록 크기가 16 의 배수가 아니면 마지막 15바이트 미만의 초과 읽기는 보이지 않는다. 이번 단계 버퍼는 16 화소(32·64 바이트)라 모두 16 의 배수다. 화소 수가 다른 프레임에서의 초과는 다른 크기로 반복하지 않았다.
- **입력이 float32 로 표시된 프레임 + 게인 bypass**: 변환은 입력이 uint16 일 때만 한다. float32 표시 입력은 예전처럼 그대로 넘기고 방어 검사(float32·N×4)를 통과하는지로 판정한다. 그런 호출은 시험하지 않았다(파이프라인의 문서화된 입력은 uint16).
- **구조가 깨진 설정의 거부**는 새 동작이다(5절 호환성 메모). 현장 구형 XCal 에서의 영향은 점검하지 못했다.
- **파이프라인 설정 JSON 의 첫 출현 한계**는 실측으로 확인했을 뿐 고치지 않았다(6절).
- `ci-preprocess` 구성 하나, Windows/MSVC 에서만 돌렸다.

## 10. 잔여 위험

- 게인 bypass + 비닝/결함/고스트 구성은 이전에는 거의 틀린 결과를 냈으므로(위 40 구성) 이 구성을 쓰던 호출자가 있었다면 결과가 달라진다 — 이제 맞는 값(게인 1)이다.
- `final_result_is_float` 와 실제 단계 조건이 다시 어긋나면 최종 형식 검사가 `PROCESSING_FAILED` 로 막는다. 조합 전수 시험이 그 어긋남을 가장 먼저 잡는다.
