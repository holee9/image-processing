[2026-10-17 19:46 KST] 부호 있는 offset 품질 측정 설계 (QA-A-263 할 일 3, Codex #232 R232-2) — 설계 문서만, 구현은 리더 승인 뒤

이슈 #273. 이 문서는 코드를 바꾸지 않는다. 기존 `xpe_verify_offset`(UINT16 보정 영상을 받는 함수)의 의미를 몰래 바꾸지 않는다는 조건에서, 클램프 전의 부호 있는 잔차로 DarkBias 등을 재는 경로의 계약을 정한다.

## 1. 문제 (측정된 사실만)

- `xpe_verify_offset` 는 보정된 영상을 **UINT16** 으로 받는다(`xpe_verify_metrics.cpp` 의 `corrected` 는 `const uint16_t*`). offset 단계가 0 으로 자르고 정수로 반올림한 뒤라서 음수가 사라진 값이다. 소스 주석도 지금의 DarkBias 가 음수가 될 수 없음을 인정한다(`std::abs` 는 효과가 없다고 적은 곳).
- 안 쓴 다크 `Dark_16` 을 같은 offset 맵으로 보정하면, 원시 프레임의 가장 어두운 10분위(FUNC-035 규칙)에서 **부호 있는 평균은 −4.0542**, 라이브러리 출력(UINT16)으로 잰 DarkBias 는 **+0.1875** 이다(QA-A-261, Codex #232 가 실제 DLL 호출로 같은 값 재현). 한계 ±5 에 대한 여유는 실제로 0.95 인데, 라이브러리 값으로는 4.81 처럼 보인다. 같은 방식으로 `Dark_15` 는 부호 있는 +1.6494, 라이브러리 +2.3395.
- 정보가 사라지므로 검증 함수가 입력만으로 복원할 수 없다. 서로 다른 과보정 상태가 모두 0 으로 몰리는 입력이 같은 값을 낸다.
- 지금 DarkBias 의 정의가 **세 갈래**다: (i) 라이브러리 `xpe_verify_offset` = 원시 프레임의 어두운 10분위 영역에서 **UINT16 보정 영상** 평균(잘림·반올림 뒤), (ii) 시험 GUI(`clients/ImageProcTest/Services/MetricsComputationService.cs`)는 라이브러리를 부르지 않고 **파이프라인 최종 출력(float) 전체 표본의 평균**을 `<= 5` 로 비교한다(ROI 없음, 절댓값 없음), (iii) 프로토콜(`Preprocessing-E2E-Automated-Evaluation-Protocol.md` 5.2)은 `DarkBias = mean(Y_dark_roi)`, `ClampRate = count(R - D < 0) / pixel_count`(클램프 **전** 양)로 쓴다. IAP 문서의 +2.4/−3.4 는 네 번째 정의(유효 영역 전체 부호 있는 평균, 261 의 V3)다.
- `ClampRate` 는 SRS FUNC-016 과 프로토콜에 이름이 있지만 라이브러리 구조체 `XpeCalibrationMetrics` 에는 필드가 없다.

## 2. 설계 결정 (권장안과 대안)

**권장: 새 API 를 추가하고 기존 함수의 의미는 그대로 둔다.** 이유는 세 가지다. ① 기존 UINT16 함수와 구조체를 쓰는 호출자(시험·클라이언트)가 값이 바뀐 것을 모른 채 다른 양을 읽는 일이 없다. ② `XpeCalibrationMetrics` 에 필드를 덧붙이면(#223 에서 한 방식) 이전 헤더로 빌드한 호출자는 더 작은 구조체를 할당한 채로 호출해 라이브러리가 그 너머에 쓰게 된다(이 구조체의 ABI 잠금과 같은 위험). 새 결과는 **새 구조체**에 담는다. ③ 판 올림(같은 이름에 다른 입력 형식)은 헤더만 보고 다시 빌드하지 않는 호출자에게서 조용히 오동작하며, `xpe_verify_gain` 의 ABI 변경(QA-A-192)과 같은 종류의 사고다.

| 안 | 내용 | 평가 |
|---|---|---|
| **A (권장)** `xpe_verify_offset_signed` | 원시 UINT16 프레임과 **같은 offset 맵 스냅숏**을 라이브러리가 호출 시점에 잡아 안에서 `raw − offset`(float, 잘림 없음, +0.5 없음)을 계산해 지표를 만든다. 호출자는 부호 있는 잔차를 따로 만들 필요가 없다. 결과 구조체에 스냅숏 식별자를 담아 호출자가 파이프라인 실행과 같은 맵이었는지 확인할 수 있다. | ABI 가 작다(큰 float 버퍼가 DLL 경계를 넘지 않는다). 맵 재적재 경쟁은 식별자 비교로 잡는다. |
| B | 잔차를 만드는 `xpe_offset_residual(raw, out_float)` + 검증 함수가 float 잔차를 받음 | 호출자가 중간 영상을 얻을 수 있어 진단에는 유용하지만, 두 호출 사이 맵이 바뀌면 불일치(같은 스냅숏 보장을 호출자가 져야 한다). 파이프라인 단계 노출이 커진다. |
| C | 기존 함수의 입력을 FLOAT32 도 받게 하고 형식으로 의미 전환 | 같은 이름이 두 의미를 가진다. 기각. |

권장 A 의 계약 (이름은 제안이며 확정은 리더):

```text
XPE_API XpeErrorCode xpe_verify_offset_signed(
    const XpeImageBuffer*   raw_image,       // UINT16 원시 다크, 현재 적재된 offset 맵과 같은 크기
    const XpeImageMetadata* metadata,
    XpeOffsetQuality*       quality);        // 새 구조체. 호출 전 0 으로 채우지 않아도 된다(함수가 {} 로 초기화)

typedef struct XpeOffsetQuality {
    uint32_t struct_size;            // 호출자가 sizeof 를 넣는다: 이후 필드가 늘어도 작은 구조체를 덮어쓰지 않는다
    uint32_t measured_mask;          // XPE_OFFSETQ_* 비트: 어느 값이 실제 측정인지(자리표시값이 아닌지)
    uint64_t offset_snapshot_id;     // 이 호출이 읽은 offset 맵의 식별자(적재할 때마다 바뀐다)
    // 부호 있는 값 (클램프 전, float 연산; ROI = 원시 프레임의 어두운 10분위, 엄격한 <, 최소 1 %)
    double   dark_bias_signed;       // mean(raw − offset) over ROI        ← FUNC-016 의 DarkBias
    double   dsnu_adu_signed;        // std(raw − offset) over ROI          ← DSNU_ADU
    double   dark_reduction_db;      // 20·log10(std(raw ROI) / max(std(raw − offset ROI), ε))
    double   clamp_rate_pct;         // 100·count(raw − offset < 0) / 전체 화소 수  ← 프로토콜 ClampRate (클램프 전)
    // 출력 형식(UINT16, +0.5 반올림, 0 바닥) 기준의 동반 값: 이전 함수의 값과 연속성
    double   dark_bias_output;       // mean(floor(clip(raw − offset + 0.5, 0, 65535))) over ROI
    double   zero_output_rate_pct;   // 100·count(floor(raw − offset + 0.5) <= 0) / 전체 화소 수
    uint32_t roi_pixels;
    uint32_t reserved[5];
} XpeOffsetQuality;
```

- 오류: 입력이 NULL·UINT16 아님·맵 크기와 다름 → `XPE_ERR_INVALID_INPUT` / `XPE_ERR_UNSUPPORTED_FORMAT`; offset 맵이 적재되지 않음 → 기존 단계들과 같은 코드(`XPE_ERR_NOT_INITIALIZED`/적재 없음). ROI 가 1 % 미만 또는 원시 프레임에 변화가 없음 → `XPE_OK` 로 돌려주되 해당 `measured_mask` 비트를 비우고 값은 자리표시(0)이며 합격 판정에 쓰지 않는다(FUNC-035·036 의 "측정 불가는 합격이 아니다"를 따른다).
- 합격 판정은 이 함수가 하지 않는다(숫자만 낸다). 판정은 SRS FUNC-016(|DarkBias| ≤ 5 **또는** DarkReduction ≥ 10 dB)을 읽는 쪽이 하며, 어느 쪽 값(부호 있음/출력)으로 판정했는지 기록한다.
- 기존 `xpe_verify_offset`, `XpeCalibrationMetrics` 는 **바꾸지 않는다.** 문서에 "출력 영상 기준(post-clamp) 값이며 부호 있는 값이 아니다"를 명시하고 `dark_bias` 의 설명을 정정한다(주석 변경만).
- 필요한 내부 변경: offset 맵 스냅숏이 이미 파이프라인 단계가 쓰는 보관소의 스냅숏 기구(`xpe_offset_correct` 가 호출 시점에 잡는 것)를 재사용한다. 식별자는 적재 시 증가하는 64비트 세대 번호로 충분하다.

## 3. 지표별로 어느 처리 단계의 값인가

| 지표 | 프로토콜 정의 | 지금 라이브러리(`xpe_verify_offset`) | 지금 시험 GUI | 제안(`xpe_verify_offset_signed`) |
|---|---|---|---|---|
| DarkBias | `mean(Y_dark_roi)` | UINT16 보정 영상(잘림·반올림 뒤)의 어두운 10분위 평균 → 음수 불가 | 최종 출력(float) 전체 표본 평균, ROI·절댓값 없음 | `raw − offset`(float, 잘림 전)의 어두운 10분위 평균 = `dark_bias_signed` |
| DSNU_ADU | `std(Y_dark_roi)` | UINT16 보정 영상 ROI 의 표준편차(0 근처에서 잘려 줄어든다) | 최종 출력 전체 표준편차 | 부호 있는 잔차 ROI 의 표준편차 = `dsnu_adu_signed` |
| DarkReduction_dB | `20·log10(std(R_roi)/max(std(Y_roi), ε))` | R = 원시, Y = UINT16 보정 영상 | 원시 대 출력 전체 | R = 원시 ROI, Y = 부호 있는 잔차 ROI |
| ClampRate | `count(R − D < 0)/N`(클램프 **전**) | **필드 없음** | 최종 출력에서 0 인 화소의 비율 | `clamp_rate_pct`(클램프 전 음수 비율), 출력 기준 0 비율은 `zero_output_rate_pct` 로 따로 |

각 값의 처리 단계(클램프 전/후)를 이름에 드러낸다(`_signed`, `_output`). 한 구조체 안에서 일부는 클램프 전, 일부는 후인데 같은 도메인 값처럼 쓰지 않는다.

## 4. 호출자와 문서

- **시험 GUI** `clients/ImageProcTest/Services/MetricsComputationService.cs`: 라이브러리 지표를 부르지 않고 자체 계산(위 표). 새 함수가 승인되면 다크 지표를 새 함수로 바꾸고, 절댓값을 쓰며, ROI 규칙을 FUNC-035 와 같게 한다. 이 변경은 GUI 레인 카드로 별도(제품 코드 아님).
- **문서**: SRS FUNC-016(부호 있는 DarkBias, ClampRate 정의를 클램프 전으로 못 박음)·FUNC-035(ROI 규칙 유지)·FUNC-037(ABI 잠금: 새 구조체는 기존 잠금 밖), 프로토콜 5.2, `api-spec.md`, IAP(DarkBias 정의 V1/V2/V3 구분 — 261 보고서 정정 초안), 헤더 주석.
- 외부 호출자 검색: 저장소 안에서 `xpe_verify_offset` 을 부르는 곳은 시험과 문서뿐이다(검색 범위: `image-processing`, `xpe-gui`, `xpe-pre` 의 .cs/.cpp/.h/.py, 그 밖은 `iop-algorithm` 의 별개 Python 호스트가 이름을 언급). 외부 배포 호출자는 확인하지 못했다(공백으로 적는다).

## 5. 시험 목록 (구현 시 같이)

1. **같은 맵 스냅숏**: 호출 사이에 다른 offset 맵을 적재하면 `offset_snapshot_id` 가 달라진다; 식별자가 같은 두 호출은 같은 값. 호출 중에 적재가 끼어들어도 한 호출 안의 값은 한 맵에서 나온다.
2. **음의 편향**: 맵이 원시보다 큰 과보정 입력에서 `dark_bias_signed < 0`, 그리고 `Dark_16`(−4.0542)과 `Dark_15`(+1.6494)를 numpy 와 소수 4자리까지 비교(회귀 시험의 골든).
3. **양의 편향**: 맵이 원시보다 작은 입력에서 `dark_bias_signed > 0`.
4. **클램프 비율**: 평균 0 정규 잡음 입력에서 `clamp_rate_pct ≈ 50`, 전부 양수 입력 0, 전부 음수 입력 100; 출력 기준 값과의 관계(`dark_bias_output ≥ dark_bias_signed`, 차이는 σ/√(2π) 근방).
5. **경계 반올림**: `raw − offset` 이 정확히 −0.5, 0, +0.5 인 화소에서 출력 기준 값이 라이브러리 `xpe_offset_correct` 의 출력과 비트 동일(같은 연산 순서).
6. **ROI 부족**: 동점 때문에 10분위 후보가 1 % 미만인 프레임 → `measured_mask` 에서 DARK_BIAS 비트가 비고 값은 자리표시, 자동 합격 아님.
7. **비유한 입력**: 맵에 NaN/Inf 가 있거나 크기가 다르면 오류 또는 측정 불가, 값이 숫자로 새지 않는다.
8. **기존 함수 회귀**: `xpe_verify_offset` 의 값이 이전과 같다(`Dark_15` +2.3395, `Dark_16` +0.1875), 구조체 크기·오프셋 정적 단언 불변.
9. **ABI**: 새 구조체 크기·필드 오프셋 정적 단언, `struct_size` 가 작은 호출자에게는 있는 필드까지만 쓴다.
10. **스레드**: 두 스레드가 다른 크기·다른 맵으로 동시에 호출해도 서로 값이 섞이지 않는다.

## 6. 열린 품질 문제로 기록: 출력 UINT16 0 클램프의 저신호 영향

출력 UINT16 의 0 클램프는 이번 설계에서 유지한다(호환성). 그러나 지표만의 문제가 아니다. 평균 0, 표준편차 σ 인 잡음 X 에 0 클램프를 하면 평균이 σ/√(2π) ≈ 0.40σ 로 올라가고 분산도 바뀐다(분포 적분). QA-A-261 에서 보정한 안 쓴 다크 2장이 실측으로 +0.51(화소 24 % 가 잘림)과 +3.65(83 % 가 잘림)였다. 밝은 평가 프레임에는 잘림이 0 이었지만, 그것이 다크 근처 저신호 영역의 선형성·잡음 무영향을 입증하지는 않는다. 가능한 방향(별도 요구·영상 품질 검증으로 판단): 내부를 signed float 로 처리하고 끝단에서 pedestal 을 더해 정수화, 또는 명시적 pedestal 을 두 단계 사이에 둔다. **이번 카드에서 정하지 않으며**, 저신호 요구와 영상 품질 검증이 선행 조건이다.

## 7. 리더가 정해 줄 것

(1) 안 A(권장) 채택 여부와 함수·구조체 이름. (2) `ClampRate` 를 프로토콜대로 클램프 전 양으로 쓸지(권장) 출력 기준을 주 값으로 할지. (3) 시험 GUI 전환을 어느 카드에 넣을지. (4) 출력 클램프 문제(6절)를 어느 요구에 묶을지. 승인 전에는 구현하지 않는다.
