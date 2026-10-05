# B6 의 나머지 승인 지표: 방법 (결과 없음)

QA-A-241e, 커밋 1. 이 파일과 측정 하니스(`A241Measure.DISABLED_B6_Acceptance`)는 **결과를 하나도 보지 않은 채** 먼저 커밋한다. 결과는 나중 커밋에 들어간다. 이 커밋 뒤에 방법을 바꾸면 보고서에 "사후 수정" 으로 따로 적는다.

Codex #160 이 맞다: 241d 는 §5.3 의 acceptance defaults 가운데 `FlatResidualPct` 만 쟀고 `FPN_Reduction_dB` 와 `LineArtifactScore` 는 재지 않았다. 241d 의 `FlatResidualPct` 결과(평탄 6 보류 0.844 %)는 바꾸지 않고 그대로 둔다.

## §5.3 원문 (main 의 `Preprocessing-E2E-Automated-Evaluation-Protocol.md`)

```text
PRNU_CV = std(Y_flat_roi) / max(mean(Y_flat_roi), epsilon)
FlatResidualPct = 100 * PRNU_CV
FPN_Reduction_dB = 20 * log10(std(R_flat_roi) / max(std(Y_flat_roi), epsilon))
LineArtifactScore = max(std(row_mean(Y)), std(col_mean(Y))) / max(std(tile_mean(Y)), epsilon)
```

`Y_flat_roi` / `R_flat_roi`: ROI 는 gain 맵 값이 유한하고 0 보다 큰 **모든 화소**, 중앙 자르기와 가장자리 제외 없음.

최소 신호 수준 문단: "a flat frame is used to judge `FlatResidualPct` only when its mean over the ROI is at least **2000 ADU**."

Acceptance defaults:
- Phase 1 target: `FlatResidualPct <= 1.0%`;
- `FPN_Reduction_dB >= 10 dB` for real fixture evidence when raw non-uniformity exists;
- line artifact score shall not increase by more than 10% from the pre-gain corrected image.

## 프로토콜이 정하지 않아서 이 문서가 정하는 것

프로토콜은 아래를 정의하지 않는다. 각 선택은 이 문서가 한다 (리더 확인 대상). 어느 것도 결과를 보고 고르지 않는다.

1. **`R_flat_roi` (FPN 의 분자)**: 보정하지 않은 **원시 입력 프레임**(파이프라인에 넣은 uint16 프레임 `dark + 평탄`). §5.2 의 `R_dark_roi` 와 같은 뜻(`R` = 원시)이고 acceptance 문구도 "raw non-uniformity" 라고 쓴다. `Y` 는 출하 경로 **최종 출력**(결함 단계 뒤). 같은 ROI 화소에서 잰다. 참고로 `R` 을 "offset 보정 뒤·gain 전" 영상으로 바꾼 값도 함께 보고한다 (판정에는 쓰지 않는다).
2. **`LineArtifactScore` 의 `row_mean` / `col_mean` / `tile_mean`**: ROI 화소만으로 행·열·타일의 평균을 낸다. 행 평균은 행마다 하나(3072개), 열 평균은 열마다 하나(3072개), 타일 평균은 겹치지 않는 **64×64** 타일마다 하나(ROI 화소가 하나도 없는 타일은 뺀다). `std` 는 모집단 표준편차, `epsilon` 은 1e-12. **타일 크기 64 는 이 문서의 선택**이다 (프로토콜에 없다). 민감도를 보이려고 32 와 128 도 같이 보고하되 판정은 64 로 한다.
3. **"gain 전 보정 영상" (LineArtifactScore 의 비교 대상)**: 같은 출하 경로에서 `bypassGain` 만 켠 출력, 즉 **offset 보정 + 결함 보정까지 하고 gain 만 뺀** 영상. 이유: 결함 단계가 채우지 않은 영상은 65 k 근처의 결함 화소가 행·열·타일 평균을 지배해서 gain 의 효과를 보는 비교가 아니게 된다. 참고로 `bypassGain` 과 `bypassDefect` 를 모두 켠 "offset 보정만" 영상으로 낸 값도 함께 보고한다 (판정에는 쓰지 않는다). 이 보정 전 영상에서는 §5.3 ROI(같은 화소)를 쓴다.
4. **합격 규칙**:
   - `FPN_Reduction_dB >= 10`.
   - `LineArtifactScore(최종 출력) <= 1.10 × LineArtifactScore(gain 전)` (10 % 넘게 늘지 않음).
   - `FlatResidualPct <= 1.0 %` (241d 결과 그대로, 이번에 다시 재서 확인만 한다).
5. **대상 장과 최소 신호 수준의 범위**: 보류 시험은 241c·241d 와 같다 — 평탄 4·5·6 중 두 장으로 만든 gain 을 남은 한 장에 적용(세 조합 모두). §5.3 의 최소 신호 수준 문단은 "judge `FlatResidualPct`" 만 말한다. 그래서 **`FPN_Reduction_dB` 와 `LineArtifactScore` 는 2000 ADU 면제를 받지 않는다고 문자 그대로 읽고, 세 장 모두에서 판정한다**(주 판정). 면제를 이 두 지표로 넓혀 읽으면 판정 장이 평탄 6 한 장이 되는데, 그 읽기는 "민감도" 로만 같이 보고한다. 면제 범위를 넓히는 것은 프로토콜 변경이라 사용자 결정이다.
6. **B6 판정**: 주 판정 규칙(위 5번의 문자 그대로 읽기)에서 세 지표 중 하나라도 기준을 못 넘으면 B6 는 **실패**로 적는다. 판정에서 빼는 장·지표는 없다. 실패 시 어느 지표가 어느 장에서 못 넘었는지 적고, 기준·면제 범위를 바꾸자는 제안은 결과와 분리된 "제안" 절에만 쓴다.
7. **입력 구성과 바이너리 식별**: 241d 와 같다. offset 은 제품 생성기가 `dark.raw` 한 프레임에서, 결함 맵은 `BPMap.map`, 남긴 장의 원시 프레임은 `dark + 평탄`(uint16 합, 평탄 파일의 65 k 값은 합이 감겨서 그 화소의 원시 값이 작아진다 — 결함 맵 화소이고 결함 단계가 채운다), 설정 `{"bypassTemp":true,"bypassNonlinearity":true,"bypassBinning":true}`, 고스트 없음, 세 맵은 한 세션 ID. 측정 실행의 연속 로그에 재빌드 명령, `xpe_preprocess.dll`·시험 실행 파일 SHA-256, 방법 커밋이 조상임을 확인한 줄을 남긴다.

## 하니스가 이 식과 같은 식임을 보이는 방법

하니스(`test_zz_a241_measure.cpp` 의 `DISABLED_B6_Acceptance`)는 위 식을 그대로 구현하고, **합성 입력에 대한 손계산 대조**를 측정 전에 같은 실행 앞머리에서 낸다: (a) 평평한 영상에 한 행만 +1 을 더한 영상, 평평한 영상에 한 열만 +1 을 더한 영상의 `LineArtifactScore` 가 손계산한 값(예: 8×8 영상)과 같은지, (b) 표준편차가 2 배 줄어든 영상의 `FPN_Reduction_dB` 가 20·log10(2) = 6.0206 인지. 이 대조가 틀리면 측정 결과는 쓰지 않는다.

## 예측 (결과를 보기 전에 적는 것)

- **`FPN_Reduction_dB`**: 원시 프레임에는 암 대좌의 패턴, 평탄의 큰 규모 음영, 그리고 합이 감겨 작아진 결함 화소(약 0.25 %)가 들어 있어서 `std(R)` 이 크다 (대략 50~120 ADU). 최종 출력의 `std(Y)` 는 `FlatResidualPct × 평균` (평탄 6: 약 18.5 ADU, 평탄 4: 약 9.3 ADU). 예측: 세 장 모두 약 14~17 dB 로 **10 dB 를 넘는다**. (`R` 을 offset 보정 뒤로 바꾸면 값이 달라지므로 이 예측은 위 1번의 선택에 대한 것이다.)
- **`LineArtifactScore`**: gain 전 영상은 평탄 영상의 27 % 큰 규모 음영이 행·열·타일 평균 모두를 지배한다. gain 뒤 영상은 화소 규모 성분이 남는다. 화소 잡음만 있는 영상은 행 평균(3072 화소 평균)의 표준편차가 σ/√3072 ≈ 0.018σ, 64×64 타일 평균의 표준편차가 σ/64 ≈ 0.0156σ 라서 점수가 약 1.16 이다. 그에 비해 완만한 기울기 영상은 행·열 평균이 타일 평균보다 작거나 비슷해서 점수가 1 이하다. 예측: gain 뒤 점수가 gain 전보다 **10 % 넘게 늘어 이 기준을 못 넘을 가능성이 높다** (확신은 없다: 잔여의 큰 규모 성분 0.12~0.15 % 가 점수를 얼마나 바꾸는지 모른다). 못 넘으면 B6 는 실패다.
- 예측이 틀려서 세 지표가 모두 기준을 넘으면 B6 는 통과다. 어느 쪽이든 결과를 그대로 적는다.
