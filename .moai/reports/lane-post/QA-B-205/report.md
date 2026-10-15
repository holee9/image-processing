# QA-B-205 — `xpe_edge_enhance` 의 `amount == 0` 조기 반환이 0 하한과 유한성 검사를 건너뛴다 (#251, Codex #106)

카드: `.moai/lanes/post/inbox/QA-B-205.md`. 근거: `.moai/state/codex-archive/106.md` 발견 1(보통).

## 0. 결과

| 항목 | 내용 |
|---|---|
| 바뀐 코드 | `edge_enhance.cpp`: `amount == 0` 이 곧바로 `XPE_OK` 를 돌려주던 줄을 없애고, 같은 자리에서 **유한성 검사 → (블러 없이) 음수 화소를 0 으로 → 반환** 으로 바꿨다. 음수 화소가 없는 유한 입력은 버퍼를 쓰지 않는다(빠른 경로 유지, 블러는 만들지 않는다). 헤더 `xpe_edge_enhance` 의 `@param img` 와 `@return` 문장을 이 계약으로 고쳤다(옛 "amount == 0.0 is accepted and returns XPE_OK without modifying the buffer" 를 대체). |
| 시험 | `EdgeEnhanceZeroAmount` 4개(`test_edge_enhance_formula.cpp`). 기존 `EdgeEnhance.ZeroAmount_ImageUnchanged`(비음수)는 그대로 통과. |
| 반증 | 6팔 모두 터졌고 소스는 바이트 단위로 복원(`arms_out.txt`). |
| 16비트 소비자 불변 | **측정, 180 경우, 16비트 변환 뒤 달라진 화소 0개**(`equiv_16bit_out.txt`, §3). |
| 검증(관측) | ci-post ctest(CI 필터) 1370개 통과·0 실패(루트 `schemas/` 를 임시로 놓았을 때), `EdgeEnhance*` 포함 57개 통과, doxygen 경고 0, 헤더 점검 20개 0건. |

## 1. 바뀐 동작
- **전**: `amount == 0` 이면 파라미터와 이미지 서술자 검사 뒤 곧바로 `XPE_OK`. 음수 FLOAT32 입력은 음수 그대로, NaN/Inf 입력도 `XPE_OK`.
- **후**: `amount == 0` 도 `amount > 0` 과 같다. NaN/±Inf 화소는 `XPE_ERR_INVALID_INPUT`(같은 코드, 버퍼는 건드리지 않음), 음수 화소는 0 으로, 그 외는 그대로. 비 FLOAT32 이미지는 두 경우 모두 `UNSUPPORTED_FORMAT`(서술자 검사가 앞에서 하므로 amount 와 무관, 시험으로 확인).
- 검사는 `amount == 0` 경로에서 **블러 상수와 링 버퍼 할당 앞**에 일어난다. `amount > 0` 경로의 `peak * 12 > FLT_MAX` 오버플로 검사는 곱 `amount·diff` 를 막기 위한 것이라 `amount == 0` 에는 해당하지 않아 넣지 않았다(출력은 입력 또는 0 뿐).

## 2. 시험 (`EdgeEnhanceZeroAmount`)
- `ANegativePixelIsReturnedAsZeroAndTheRestIsUntouched`: 음수 화소를 섞은 영상(7번째마다 −3.5…−7.5, 나머지 양수). amount 0 → 출력에 음수 0개, 음수였던 화소는 전부 0, 비음수 화소는 한 개도 변하지 않음(블러가 돌지 않았다는 증거).
- `ANonFinitePixelIsRefusedWithTheSameCodeAsForEveryOtherAmount`: NaN·+Inf·−Inf 각각. 대조군 amount 0.5 가 `INVALID_INPUT`, amount 0 이 같은 코드(전: `XPE_OK`), 그리고 **같은 영상에 음수 화소도 하나 넣어** 거부된 호출은 버퍼 전체를 그대로 두는지(음수 화소가 0 으로 올려지지 않음)도 단언.
- `AFiniteNonNegativeImageIsStillReturnedByteForByte`: 유한·비음수 영상(−0.0 한 화소 포함 — 음수 영 은 0 미만이 아님)은 `memcmp` 로 바이트 동일.
- `ANonFloat32ImageIsRefusedWhateverTheAmount`: UINT16 영상은 amount 0·0.5 모두 `UNSUPPORTED_FORMAT`. (카드의 (d) "UINT16 경로가 있다면 amount=0 영향 없음".)

## 3. 16비트 소비자 불변 — 측정 (`equiv_16bit.py.txt`)
하한이 없는 바이너리(M2b 상태, `amount == 0` 조기 반환 포함)와 새 바이너리를 같은 입력에 돌려 gui `EnhanceBasicStage` 와 같은 변환(half-even 반올림, 0..65535 자름) 뒤 화소 단위로 비교했다. 장면 4개(해부학, 콜리메이터 띠, 피부선, 어두운 바닥 30 대 밝은 4000) × 영역 3가지(log 영역, 선형 영역, **음수 원본**: 장면 − 1500 을 log 없이 noise → CLAHE → USM 에 넣음) × amount 0·0.5·1·2·5 × threshold 0·10·100 = **180개 경우**.

| 항목 | 값 |
|---|---|
| 16비트 결과가 달라진 경우 | **0 / 180** (달라진 화소 합계 0) |
| 하한 없을 때 음수 float 가 있는 경우 | 135 / 180 |
| 하한 있을 때 음수 float 가 있는 경우 | 0 / 180 (가장 낮은 값 0.000) |
| `amount = 0`, 음수 원본 경우 | 해부학 556개·콜리메이터 2277개 음수 화소가 float 에서만 0 으로 바뀌고 16비트로는 0 화소가 다름(세 임계값 모두) |

## 4. 반증 (`arms_driver.py.txt`)

| 끈 방어 | 빨개진 시험 |
|---|---|
| 조기 반환을 원래 자리로(블록 전체를 `return XPE_OK` 로) — 카드의 (3) | 음수 시험, 비유한 시험 |
| 유한성 검사 결과를 무시 | 비유한 시험 |
| `amount == 0` 의 0 하한 제거 | 음수 시험 |
| 하한을 1 로 | 음수 시험 |
| 비음수 화소도 건드림(`+1`) | 음수 시험(그리고 20 ms 단발 성능 시험이 함께 흔들림) |
| 음수가 없어도 쓰기 경로로(전 화소에 곱) | 바이트 동일 시험, 음수 시험 |

(`EdgeEnhance.Performance_3072x3072_Within20ms` 가 한 팔에서 함께 빨갰다 — 단발 20 ms 시험이라 재빌드 직후 기계 상태에 흔들린다. 이 시험은 `amount > 0` 경로를 재므로 이번 수정과 무관하다.)

## 5. Gap / 잔여 위험
Gap
- `amount == 0` 경로에서 큰 영상(3072²)을 읽는 시간은 재지 않았다(유한성 검사 한 번의 패스, 이전 `amount == 0` 호출은 읽지 않고 반환했다). 호출자가 `amount = 0` 을 파이프라인에서 반복 호출하면 그만큼의 시간이 든다. 관측하지 않음.
- 요구 문구(REQ-ENH-021 하한 0)와 REQ-CHANGE-LOG 는 리더 몫이라 건드리지 않았다.
- 16비트 측정의 장면은 합성이다(M3 와 같은 한계).

잔여 위험
- `amount = 0` 으로 부르던 호출자가 NaN/Inf 를 가진 영상을 넘기면 이제 `INVALID_INPUT` 을 받는다(전엔 `XPE_OK`). 의도한 변경이고 `amount > 0` 과 같은 거동이다.
