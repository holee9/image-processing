# QA-A-216 — 고스트 보정의 비유한 입력: 요구는 "건너뜀"과 "거부" 중 무엇을 말하는가 (보고서만, Refs #233)

기준: dev/preprocess `76085cd7`. 코드 변경 없음. 증거: `evidence/10_ghost_nonfinite_measure.txt`(측정 스크립트 출력, 24 + 6행).

## 결론 (한 줄씩)

1. **입구 거부로 통일해도 SPEC-XPE-P1A 와 충돌하지 않는다.** 고스트의 비유한 입력에 대해 "건너뜀"을 정한 요구는 없다. 오히려 지금 동작이 `REQ-P1A-032` 를 어긴다(실패하는데 출력 버퍼를 일부 쓴 채 둠).
2. **상태 오염: 비유한 입력으로는 없다** (24/24 경우에서 실패한 프레임이 핸들 상태를 바꾸지 않음). 단 **유한한 극단 입력으로는 있다**(아래 4번) — 파이프라인으로는 닿지 않는 값.
3. **결함 2건**: (a) 실패 시 출력 부분 기록(`REQ-P1A-032` 위반), (b) 극단 유한 입력의 이력 오버플로가 커밋되어 이후 프레임이 전부 실패.
4. 리더가 정할 것: 입구 거부 카드(QA-A-217 후보)를 열지, 그리고 **SPEC `REQ-P1A-091` 문구 개정**(215 가 바꾼 코드와 어긋남 — 아래).

## 1. 요구 원문

검색 범위: `.moai/specs/SPEC-XPE-P1A/*.md`(spec·research·compact), `docs/ghost-correction/*.md`(srs·sdd·sad·rtm·stp·tds·prd·README), `docs/post-processing/xpe/XPE-SRS-001·SDD-001·SDD-002`, `docs/calibration/SRS-CALIB-001`, `docs/project/api-spec.md`. 패턴: ghost/lag/잔상 × NaN/Inf/finite/비유한/invalid/fail/error/state/history/skip.

| 출처 | 원문(요지, 줄) | 고스트 비유한 입력에 대해 |
|---|---|---|
| SPEC `REQ-P1A-085` (`spec.md:679`) | 핸들 생성·이력 보관·`OUT_OF_MEMORY`·destroy 가 무효화 후 해제 | 말하지 않음 |
| `REQ-P1A-086` (`:687`) | 잘못된 핸들 → `INVALID_INPUT` | 말하지 않음 |
| `REQ-P1A-087` (`:695`) | FLOAT32 버퍼에 이력 기반 lag 기여를 **제자리에서** 뺀다 | 말하지 않음 |
| `REQ-P1A-088` (`:702`) | reset 이 이력·노출 상태를 비운다 | 상태 정의만, 오류 시 상태 언급 없음 |
| `REQ-P1A-032` (`:905`) | *"shall not leave output buffers in a partially initialized state on error. On failure, … either leave the output unmodified or zero-fill it entirely."* | **오류 시 출력은 불변이거나 전부 0** — 건너뜀·부분 기록은 허용되지 않음 |
| `REQ-P1A-033` (`:912`) | *"shall not produce NaN or Inf … validation to clamp or replace invalid values"* | 출력 NaN/Inf 금지. 입구 거부도 만족(출력을 만들지 않음). "clamp or replace" 문구는 거부를 막지 않으나 대체를 권하는 것으로 읽힐 수는 있음 |
| `REQ-P1A-098/099` | 핸들 없으면 건너뜀 / 단계는 자기 복사본을 받음 | 비유한과 무관 |
| `docs/ghost-correction/TDS-GHOST-001` §7.2.1 | NaN 화소 → *"Replace with median of neighbors OR 0 … log count … output valid"* | **이 문서만 "대체" 쪽**. 시험 데이터 명세이고 옛 설계(오프셋 보정 단계, CORR_* 오류 체계)를 서술 — 출하 SPEC 요구가 아님 |
| `srs_ghost_correction.md` FR-xxx, `sdd_ghost_correction.md` | NULL 포인터·CRC·dark frame 만 | 비유한 언급 **없음** |
| `XPE-SRS-001`·`SDD-001/002`·`SRS-CALIB-001` | (ghost 행에서 비유한·실패 시 상태 언급 검색) | **없음** |

**어느 요구도 "평균에서 비유한을 건너뛴다"를 정하지 않는다.** 그 건너뜀(`compute_frame_mean`, 3단계 이웃 평균)은 구현 선택이다.

## 2. 지금 동작 (실측, 8×8 프레임, 티어 1·2·3 × 첫 프레임/좋은 프레임 2개 뒤 × 4 종류)

종류: 0 번 화소 NaN / 35 번 NaN / 마지막 화소 +inf / 전부 NaN. 출처: `evidence/10_…txt`.

| 항목 | 결과 |
|---|---|
| 반환 코드 | 24/24 전부 `-3` (`XPE_ERR_PROCESSING_FAILED`) |
| 이력·시각·노출 상태 | **24/24 불변**: 실패 프레임 뒤에 같은 정상 프레임을 먹이면 (첫 프레임이면 새 핸들, 아니면 같은 이력을 쌓은 대조 핸들)과 **바이트 동일**, rc=0 |
| 출력 버퍼(입력과 바이트 비교, 바뀐 화소 수 / 64) | 좋은 프레임 2개 뒤(티어 1·2·3 동일): 비유한이 0 번이면 0, 35 번이면 **35**, 마지막이면 **63**; 전부 NaN 이면 0. 첫 프레임: 티어 1·2 는 이력이 0 이라 쓰는 값이 입력과 같아 바이트가 **안 달라 보임**(0 — 쓰지 않은 것이 아님), 티어 3 은 이웃 평균 분기 때문에 35 번 NaN → 20, 마지막 +inf → 36 |

상태가 보존되는 이유는 코드 그대로다: 새 이력은 `next1/next2` 에 쓰고 **전체 프레임이 성공했을 때만** `swap`·시각·노출을 커밋한다(`ghost_correct.cpp:272-281`, QA-A-202c). 즉 상태는 안전하고, **출력만** 안 안전하다.

호출자: `xpe_ghost_correct` 는 파이프라인 한 곳(`pipeline.cpp:418`)에서만 불리고 클라이언트·GUI 직접 호출은 없다(`grep -rn`, clients/gui/modules, 시험 제외). 파이프라인은 REQ-P1A-099 에 따라 자기 복사본(`stage7`)을 넘기고, 그 입력은 결함·비닝 단계의 유한 출력이라 비유한이 도달하지 않는다.

## 3. 입구 거부로 통일하면

- `REQ-P1A-032` 충족(출력 불변) · `REQ-P1A-033` 충족 · 215 의 세 단계(결함·비닝·런타임 검출)와 코드·알림 방식이 같아짐.
- 부수 효과: `compute_frame_mean` 과 티어 3 이웃 평균의 `isfinite` 건너뜀 분기는 도달 불가가 된다(정리 대상).
- 충돌: `TDS-GHOST-001` §7.2.1 의 "대체" 문구(비규범 시험 명세)와는 갈라진다 — 그 문서는 고쳐야 한다.
- 기능 변화: 고스트가 비유한 프레임에 `INVALID_INPUT` 을 돌려준다(지금은 `PROCESSING_FAILED`). 호출자 없음 → 영향 없음.

## 4. 이번에 나온 두 번째 결함 (유한 입력이 상태를 오염)

`n1[i] = decay1*h1[i] + raw` 는 **검사 없이 커밋**된다(`corrected` 만 검사). 유한한 극단값 입력을 두 프레임 먹이면 이력이 +inf 로 오버플로한 채 커밋되고, 이후 프레임은 `raw - a1*inf` 로 전부 실패한다:

```
finite 3e38 frame 1 rc=0 / frame 2 rc=0 / frame 3 rc=-3 / frame 4 rc=-3
ordinary clean frame after that: rc=-3          ← 평범한 프레임도 실패
ordinary clean frame after reset: rc=0          ← reset 만이 복구
```

도달성: 파이프라인의 고스트 입력은 게인 보정 뒤 값(상한 약 65535×10)이라 이력 합이 ~1e6 수준 — 오버플로(~1e38)까지 30자릿수 여유가 있다. 따라서 **직접 호출자가 극단값을 넘길 때만** 닿고, 현 호출자는 없다. 심각도 낮음, 하지만 "실패가 영구 상태가 된다"는 형태는 입구 검사(입력 유한 + 한계 이내)나 커밋 전 이력 유한성 검사로 같이 닫는 편이 값싸다.

## 5. 리더에게 (결정이 필요한 것)

1. **QA-A-217 후보 — 고스트 입구 거부**: 입력 전체를 한 번 훑어 비유한이면 `INVALID_INPUT`, 쓰기 전 거부(214b·215 와 같은 도우미 `xpe_find_nonfinite` 재사용), 알림 `XPE_WARN_GHOST_INPUT_NOT_FINITE`. 상태 불변은 이미 성립하므로 시험은 "출력 바이트 불변 + 상태 불변(정상 프레임 결과 일치)"을 고정하면 된다. 4번의 이력 오버플로는 같은 카드에서 `next1/next2` 유한성 검사로 닫을지 별건으로 할지 결정 필요.
2. **SPEC `REQ-P1A-091` 개정(리더 소유 파일)**: 문구가 *"If any pixel is non-finite during normalization, it shall return XPE_ERR_PROCESSING_FAILED"* 인데 215(76085cd7)가 코드를 **입력 검사 + `XPE_ERR_INVALID_INPUT`, 쓰기 전**으로 바꿨다. SPEC 이 코드와 어긋났다 — 215 보고서에서 짚지 못한 내 누락이다. 개정 문구 제안: *"If any pixel of the input is non-finite, it shall return XPE_ERR_INVALID_INPUT before writing any pixel."* (측정된 계약 칸: `binning_correct.cpp` 사전 검사, `test_nonfinite_inputs.cpp`).
3. `TDS-GHOST-001` §7.2.1 "대체" 문구 정리 여부.

## 미검증 (Gaps)

- 티어 3 이웃 평균이 `px[ni]` 로 **이미 보정된 윗줄 화소**를 읽는 것으로 코드상 보이지만(`:197-203` — 순서대로 제자리 갱신) 측정하지 않았다. 설계 의도(원본 이웃 평균)와 다른지는 이 카드 범위 밖.
- 큰 프레임(3072²)에서의 거동·시간은 재지 않았다(8×8 로 상태·출력 의미만 측정).
- 시험 데이터 명세(TDS) 외의 옛 설계 문서에 비유한 언급이 더 있는지는 검색 패턴 범위 안에서만 확인.
- 클라이언트가 고스트를 간접 호출하는 경로(파이프라인 경유)에서 비유한이 정말 못 도달하는지는 단계 출력의 유한성(`REQ-P1A-033`, 214b 입구 검사)에 기대는 추론이다 — 파이프라인 전체에 비유한을 주입한 시험은 이번에 하지 않았다.

## 잔여 위험

- 4번 결함은 현 호출자가 없어 잠복 상태다. 새 호출자(예: 스트리밍 입력)가 생기면 처음 닿는다.
