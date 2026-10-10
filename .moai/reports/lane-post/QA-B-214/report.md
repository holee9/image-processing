# QA-B-214 — 표시 극성 반전과 해부 영역 자동 창 구현 보고서

작성: Lane B (xpe-post), `dev/postprocess`. 제품 코드 변경 포함, Codex 검토 대상. 커밋: 제품·시험 `b796ba00`, 하니스·증거는 이 보고서와 함께 올라간 다음 커밋.
사용자 결정(#251): ① 극성은 Presentation LUT 마지막 단계에서 뒤집는다(DICOM 은 MONOCHROME2 그대로). ② 기본 창은 해부 영역 기준 자동 창이다. CLAHE 띠는 그대로 둔다.

## 결론

| 항목 | 상태 |
|---|---|
| 극성 반전 | 구현. `xpe_apply_presentation_lut` 기본이 LUT 를 거꾸로 읽는다. 옛 동작은 새 `xpe_apply_presentation_lut_ex(.., XPE_PRESENTATION_AS_IS)` |
| 자동 창 | 구현. 새 `xpe_voi_auto_window`. 실영상 창 `[1981.34, 3179.77]`, 최종 영상은 뼈 희고 공기 검음 |
| 요구 문구 초안 | `draft_1/2/3_*.txt` 세 개 |
| 증거 PNG | `xpe-data/m2-post-v2/` 8장 + `sha256.txt` + `README.txt` |
| 회귀(P1~P7) | 아래 4절. 기능 회귀 없음, P2 는 기계 부하 탓에 두 번의 실행이 흔들렸고 한 번은 조용한 상태 |
| 표시 시험 | 11개 실행 파일 146개 모두 통과(기존 131 + 신규 15), 기존 E2E 12개 통과, doxygen 오류 0 |
| 반증 | 3개 arm 모두 의도한 시험이 빨강, 복원 후 빌드·시험 초록(SHA-256 동일) |

**먼저 알려야 할 것 셋**

1. **다른 레인에 영향**: 기본 함수가 바뀌므로 `xpe_apply_presentation_lut` 를 부르는 코드는 줄을 바꾸지 않고도 뒤집힌 영상을 받는다. 확인한 호출처(읽기만, 실행 안 함): `gui/ImageProcTest/Services/BaselineDisplayStage.cs`, `Native/GuiBaselineDisplayNative.cs`, `RealXpeBackend.cs`, `clients/ImageProcTest/Services/NativePresentationExportService.cs`. `clients/ImageProcTest.IntegrationTests/Functional/BaselineDisplayNativeTests.cs` 가 화소값을 단언하면 빨개질 수 있다. 앱의 viewer "Invert"(`ViewportRenderParams.Invert`, 기본 꺼짐)를 켜면 두 번 뒤집힌다. 통보가 필요하다.
2. **MONOCHROME1 입력과 두 번 뒤집기(카드 1항)**: 코드로 확인한 것은 읽기 쪽뿐이다(`DicomReader.cpp:375` `normaliseMonochrome1` 이 `(2^B−1) − 값` 으로 뒤집어 MONOCHROME2 의미로 돌려준다). 이 값이 그대로 체인에 들어가면 뼈가 높은 값이므로 **기본 극성을 쓰면 두 번 뒤집힌다.** 호출자가 `AS_IS` 를 골라야 한다. 함수는 자료의 극성을 알 수 없다. 실제 MONOCHROME1 파일로 읽기→체인→표시를 끝까지 돌려 보지는 못했다(DICOM 은 보류, 이 빌드(ci-post)에는 dicom 모듈도 없다). 이 결정 지점은 초안 3 의 "열린 점" 1·2 에 적었다.
3. **창 위쪽 선택이 대비를 바꾼다**: 213b 에서 리더가 좋다고 본 비교 영상 05(창 위 = 오츠 문턱)보다 **대비는 낮다**(해부 화소 p05–p95 폭 57783 → 43370). 피부선 연조직이 잘리지 않게 창 위를 배경 하위 5% 로 잡았기 때문이다. 하위 1% 로 올린 대안을 `06_ALT` 로 함께 만들었다(폭 51185). 어느 쪽이 더 보기 좋은지는 육안 판단이고, 값은 명명 상수 하나(`kAutoWindowBackgroundLowQuantile`)다.

## 1. 극성

**구현** (`presentation_lut.cpp`): 입력 인덱스 `idx = clamp(round(v·1023), 0, 1023)`는 그대로이고, LUT 를 거꾸로 복사한 1024칸 표(`table[i] = lutData[1023 − i]`)를 한 번 만든 뒤 화소 루프를 돌린다. 화소 루프는 두 극성이 같아 추가 비용이 없다.

**API 제안과 결정**: 오름차순을 고르는 길이 **필요하다고 판단해 넣었다.** 이유 셋 — (a) 기존 시험 7개가 "인덱스 → `lutData[index]`" 매핑 자체를 단언한다, (b) 배경이 이미 낮은 자료(MONOCHROME1 정규화본)와 호출자가 따로 반전하는 경우가 있다, (c) 구조체 `XpePresentationLutParams` 를 바꾸면 C# P/Invoke 의 구조체 크기가 어긋난다(옛 호출자가 2052바이트를 넘기는데 새 필드를 읽으면 쓰레기 값을 읽는다). 그래서 구조체는 그대로 두고 함수 하나를 더 두었다: `xpe_apply_presentation_lut_ex(img, params, XpePresentationPolarity)` (`XPE_PRESENTATION_INVERTED = 0` 기본, `AS_IS = 1`). 알 수 없는 값은 `XPE_ERR_INVALID_INPUT`, 영상 무변경. 내보내기(export) 함수가 하나 늘었다(`xpe_apply_presentation_lut_ex`, `xpe_voi_auto_window`): 내보낸 함수 개수를 단언하는 시험이나 문서가 있으면 따라 고쳐야 한다 — 이 저장소의 display 시험에서는 개수를 단언하는 곳을 찾지 못했다(`grep exports` 결과 없음. 문서 쪽은 확인하지 않았다).

**표를 거꾸로 읽는 이유(`65535 − 값` 이 아니라)**: 램프에서는 둘이 정확히 같다(1024개 인덱스 전부 차이 0, 분모가 홀수라 반올림 동점이 없다). GSDF 표에서는 다르다 — `GsdfTableIsReadBackwardsNotSubtracted` 가 같은 표에서 `65535 − lutData[i]` 가 다른 답임을 대조군으로 단언한다. 거꾸로 읽으면 GSDF 가 보정한 간격이 보존된다. DICOM 의 Presentation LUT Shape INVERSE(PS3.3 C.11.6)와 같은 의미다(이 세션에서 표준 원문을 다시 열어 보지는 않았다).

**기존 시험 바꾼 것 7개** (각각 이유를 주석에):

| 시험 | 바꾼 방식 |
|---|---|
| `PresentationLut.LutLookup_HalfValue / ZeroInput / OneInput`, `InputClamp_Negative / AboveOne` | 인덱스 매핑을 보는 시험이라 `_ex(AS_IS)` 로 바꿨다 |
| `ReproQaB200Display.D5_GsdfEnabled…` | gsdfEnabled 가 표를 바꾸지 않음을 보는 시험이라 `_ex(AS_IS)` 로 바꿨다 |
| `DisplayIntegration.FullPipeline_LinearModality_LinearVoi_PresLut` | 기본 극성의 값 `509 = 1023 − 514` 로 바꾸고, 옛 514 는 같은 2단계 출력의 사본에 `AS_IS` 로 단언해 #156 의 인덱스 산술이 계속 덮인다 |

**새 시험 6개** (`PresentationPolarity`): 기본이 높은 쪽을 어둡게(0↔65535), 기본 = `_ex(INVERTED)` 바이트 동일, 램프 = `65535 − 오름차순` 1024개 전부, GSDF 표 거꾸로 읽기(대조군 포함), `AS_IS` = 옛 매핑, 알 수 없는 극성 거절 + 영상 불변.

**시간**(번갈아 30라운드, 같은 프로세스·같은 입력, `p2_presentation_ab_run1/2.txt`): 두 극성의 중앙값 차이 −0.06 ms, +0.34 ms. 극성은 시간을 더하지 않는다.

## 2. 자동 창 `xpe_voi_auto_window`

**방법**: ① 영상 전체를 한 번 훑어(`xpe_scan_finite`) 비유한 화소를 거절하고 최솟값·최댓값을 얻는다 ② 4행·4열 간격 표본(1/16)을 1024칸 히스토그램에 넣는다 ③ 오츠 분리: **높은 쪽 클래스 = 배경**, 낮은 쪽 = 해부 ④ 창 아래 = 해부 클래스의 0.5% 분위수, 창 위 = 배경 클래스의 5% 분위수 ⑤ 출력 `XPE_VOI_LINEAR_EXACT`, center = (아래+위)/2, width = 위−아래, `minOut 0`, `maxOut 1`(Presentation 입력 범위).
`LINEAR_EXACT` 인 이유: 창 양 끝이 정확히 `minOut`/`maxOut` 으로 가야 하고, `LINEAR` 의 반 칸 보정은 정수 자료용이다. 시험 `WindowEndsMapToTheEndsOfTheOutputRange` 가 단언한다.

**명명 상수와 근거** (모두 `voi_auto_window.cpp` 앞머리에 근거와 함께 있다; 값은 1단계 기준 영상 v2 의 손목 프레임(로그 영역, USM 뒤)과 저장소의 다른 실영상에서 정했다):

| 상수 | 값 | 근거(측정) |
|---|---|---|
| `kAutoWindowSampleStride` | 4 | 전 영상 대비 창 끝 이동이 폭의 0.02% / 0.04%. 8 → 0.11% / 0.05%, 16 → 0.12% / 0.30% |
| `kAutoWindowHistogramBins` | 1024 | 칸 폭 = 범위의 0.1%. 256~4096 칸 사이 창 끝 변화 1 로그단위 미만 |
| `kAutoWindowAnatomyLowQuantile` | 0.005 | 1% 에서는 해부 화소 0.98% 가 창 아래로 잘림, 0.5% 에서 0.50%, 0.1% 에서 0.10%(폭 0.648 대 0.662) |
| `kAutoWindowBackgroundLowQuantile` | 0.05 | 아래 표 |
| `kAutoWindowMinClassFraction` | 0.02 | 손목 해부 11.7%, 평탄장 영상은 죽은 화소가 0.8~1.3% |
| `kAutoWindowMinSeparability` | 0.75 | 손목 0.85~0.89, 평탄장 0.60~0.72, 한 조직 합성 픽스처 0.67, 종 모양 분포 이론값 2/π = 0.64 → 0.72 와 0.85 사이 |
| `kAutoWindowFallbackLow/HighQuantile` | 0.01 / 0.99 | 213 기본 창(전체 영상 1~99%) |

**창 위쪽 선택표** (`numpy` 연구, 같은 영상, 아래 = 해부 1% 분위수, "고리" = 해부 바깥 44 화소 둘레의 피부선 전이 화소):

| 창 위 | 해부 대비(p05–p95 폭 / 창) | 고리 중 창 위로 잘린 비율 |
|---|---|---|
| 오츠 문턱(처음 설계) | 0.871 | **100%** |
| 배경 1% 분위수 | 0.795 | 53.2% |
| **배경 5% 분위수(채택)** | 0.672 | **9.9%** |
| 배경 10% | 0.630 | 4.1% |
| 배경 25% | 0.595 | 2.1% |
| 배경 중앙값 | 0.576 | 1.4% |

5% 는 "피부선 전이의 90% 를 살리고 해부에 출력 범위의 2/3 를 주는" 지점이다. 이것은 한 장의 영상으로 고른 값이다.

**대체 동작**(카드 2항 "영역이 너무 작거나 전부 해부인 경우"): 한 클래스가 표본의 2% 미만이거나 오츠 분리도가 0.75 미만이면 전체 영상 1~99% 창 + Info 경보 1건(`VOI auto window: <이유> -- …`). 평탄 영상(최댓값 = 최솟값)은 center = 그 값, width = 1.0, 같은 경보. 시험: `UnimodalImage…`(종 모양), `TinyClass…`(해부 1%), `FlatImage…`.
**한계로 적어 둔 것**: (1) 배경이 데이터의 **높은** 쪽이라고 가정한다(배경이 낮은 MONOCHROME1 정규화본에서는 반대 클래스를 잡는다), (2) 콜리메이터 그림자(해부보다 어두움)는 해부 클래스로 들어가 창을 넓힌다, (3) 영상이 거의 전부 해부이고 조직 집단이 둘이면(프레임을 채운 손) 그 안에서 "해부/배경" 으로 갈린다. 실영상으로는 손목 한 장과 평탄장 몇 장만 봤다.

**실영상 결과**: 제품 함수가 낸 창 `[1981.34, 3179.77]`(center 2580.55, width 1198.43)은 numpy 모의(표본 4)의 `[1981.37, 3180.07]` 와 0.3 로그단위 안에서 같다. 오츠 문턱 2910.29, 해부 11.66%.

**시간**(`xpe_voi_auto_window`, 3072², 반복 21회, 3회 실행): 중앙값 2.77 · 2.92 · 3.00 ms, 최대 4.13 ms, 첫 호출 2.55–4.53 ms. VOI 16 ms 예산과의 관계: 자동 창은 **별도 호출**이라 `xpe_apply_voi_lut` 의 16 ms(REQ-DISP-016)에 들어가지 않는다. 합쳐도 VOI 단계 중앙값 4.1–5.3 ms + 자동 창 약 3 ms ≈ 7–8 ms 로 16 ms 안이다. 요구 초안에는 "자동 창 ≤ 16 ms" 를 제안으로만 적었다(예산이 정해진 적이 없어 리더 결정).

## 3. 증거 영상과 수치

`xpe-data/m2-post-v2/` (README 에 명령·매개변수·SHA-256). 같은 해부 마스크(오츠, 11.66%) 기준 출력 uint16 수치:

| 영상 | 해부 화소 p05 · p50 · p95 | 폭 | 해부 화소의 0 / 65535 | 전체 영상의 0 / 65535 |
|---|---|---|---|---|
| 07 OLD(213: 공기 흼, 전체 창) | 0 · 18001 · 35939 | 35939 | 8.64% / 0% | 1.01% / 1.12% |
| 06 ALT(배경 1% 분위수 위) | 8905 · 33056 · 60090 | 51185 | 0% / 0.52% | 87.46% / 0.06% |
| **05 FINAL(새 기본)** | **17553 · 37989 · 60923** | **43370** | **0% / 0.52%** | **83.93% / 0.06%** |

새 기본은 공기를 검게(전체의 83.93% 가 0) 두고, 해부 폭을 OLD 보다 21% 늘리면서(35939 → 43370) 뼈 쪽 끝에서 잘리는 해부 화소를 8.64% → 0.52% 로 줄였다(OLD 는 밝기 0 쪽, FINAL 은 65535 쪽이 뼈 끝이다). 213b 의 영상 05(창 위 = 오츠 문턱)의 폭은 57783 이었다.
05 FINAL 은 하니스의 변형 렌더러(`v7`)와 **차이 나는 화소 0개**다(체인 경로와 변형 경로가 일치).
육안(축소 미리보기, 저자 관찰): 05 는 통상 방사선 영상처럼 뼈가 희고 배경이 검으며, 06 은 대비가 조금 더 세고 피부선이 약간 더 잘리며, 07 은 213 의 흑백 반전 영상이다. 판정은 사용자 몫.

## 4. 213 의 P1~P7 다시 돌림 (회귀)

하니스 `test_m2_measure.cpp` 를 새 기본에 맞췄다(체인 끝은 기본 `xpe_apply_presentation_lut`, VOI 창은 `xpe_voi_auto_window`; 213b 변형 렌더러는 `_ex` 로 극성을 명시). 결과 원문은 `p1_chain.txt`, `p2_times_run1..3.txt`, `p6_leak.txt`, `p2_presentation_ab_run1/2.txt`.

| # | 항목 | 판정 | 비교 |
|---|---|---|---|
| P1 | 체인 끝까지 | 통과 | 7단계 `XPE_OK`, 출력 유한, 최종 uint16 0..65535 |
| P2 | 단계별 시간 | 아래 | |
| P3 | EI/DI | 통과 | 213 과 같은 수(EI 219.8, DI(HAND) +3.42 경보 1건) — 모듈 무변경 |
| P4 | USM 상한 | 통과 | 213 과 같은 수(상한 위반 0, 음수 0, amount 0.5·5.0) — 모듈 무변경 |
| P5 | VOI 범위, Presentation 규칙 | 통과 | VOI [0,1] 밖 0건. NaN·+inf 거절 + 버퍼 불변. 유한 범위 밖(−5, 7)은 기본 극성에서 `out[0]=65535`(=`lut[1023]`), `out[1]=0`(=`lut[0]`) |
| P6 | 100 프레임 누수 | 통과 | 기울기 +0.00243 MiB/프레임(213 은 +0.00234), 양성 대조군 +1.006. 이번에는 프레임마다 자동 창을 부른다 |
| P7 | 사람이 볼 PNG | 통과(산출) | `m2-post-v2` |

**P2 (ms, 3072², 반복 21회, 3회)**: 실행 1·2 는 기계가 바쁜 상태였다 — 변하지 않은 코드(CLAHE 중앙값 53.5·52.6, 노이즈 41.6·42.6)가 213 의 조용한 실행(CLAHE 45.5–47.2, 노이즈 36.4–37.6)보다 느렸고 최소값도 올라갔다. 실행 3 이 조용한 실행이다.

| 단계 | 한도 | 실행 3(조용) 중앙값 · 최대 | 실행 1 · 2 중앙값 |
|---|---|---|---|
| 로그 | 15 | 5.50 · 7.39 | 5.91 · 5.65 |
| 노이즈 | 100 | 38.00 · 49.49 | 41.59 · 42.56 |
| CLAHE | 50 | 46.47 · 53.56 (초과 1/21) | 53.52 · 52.58 |
| USM | 20 | 19.51 · 21.85 (초과 5/21) | 19.64 · 23.57 |
| Modality | 20 | 3.88 · 5.07 | 4.23 · 5.62 |
| **VOI** | 16 | **4.10** · 5.50 | 4.47 · 5.32 |
| Presentation | 25 | 23.61 · 26.27 (초과 1/21) | 29.75 · 27.43 |
| 자동 창(별도) | — | 2.77 · 3.45 | 2.92 · 3.00 |

- **VOI 단계가 213 의 10.7 ms 에서 4.1 ms 로 줄어든 것은 코드 개선이 아니라 모드가 바뀌어서다**: 213 은 `LINEAR`(분기 있음), 이번 체인은 자동 창의 `LINEAR_EXACT`(분기 없음). 같은 항목의 비교가 아니다.
- **Presentation 은 조용한 실행(23.61)에서 213(23.3–23.6)과 같다.** 부하 탓인지 코드 탓인지는 번갈아 측정한 A/B 로 갈랐다: 기본(반전) 대 `AS_IS` 중앙값 차이 −0.06 / +0.34 ms. 따라서 실행 1·2 의 Presentation 29.75·27.43 은 극성 때문이 아니라 부하다(이 말의 근거는 A/B 이고, 부하 자체를 직접 측정한 것은 아니다).
- CLAHE·USM 의 초과 반복은 213 에서도 있던 현상이다(213: CLAHE 2–4/21, USM 3–8/21). 이번에 코드를 건드리지 않았다.

## 5. 반증 (카드 5항)

`arms_falsification.txt` — 제품을 일부러 고쳐 빌드하고 표시 시험을 돌린 뒤 원본을 복원(SHA-256 전후 동일)하고 다시 빌드해 초록임을 확인했다.

| arm | 변형 | 빨강이 된 시험 |
|---|---|---|
| 1 | 반전을 적용하지 않음(거꾸로 읽기 대신 그냥 복사) | `FullPipeline_LinearModality_LinearVoi_PresLut`(509), `DefaultShowsTheHighEndDark`, `GsdfTableIsReadBackwardsNotSubtracted`, `InvertedRampIs…` — 4개 |
| 2 | 자동 창이 항상 전체 1~99% 창으로 대체 | `BimodalImageGivesTheAnatomyWindow` |
| 3 | 창 위를 오츠 문턱으로(처음 설계) | `BimodalImageGivesTheAnatomyWindow`, `TissueToAirTransitionStaysInsideTheWindow` |
| 복원 | — | 0개 |

주의 둘: `DefaultIsExactlyTheInvertedForm`(기본 = `_ex(INVERTED)` 바이트 비교)은 arm 1 에서 빨개지지 않는다 — 두 경로가 같은 코드를 지나므로 그 변형을 못 본다. 그 시험은 기본 경로가 `_ex` 와 같은 길을 쓴다는 것만 지킨다. `TissueToAirTransition…` 은 arm 2 에서는 초록이다(전체 1~99% 창도 전이 화소를 포함하므로). 이 시험은 arm 3 을 잡기 위한 것이다.

## 6. 요구 문구 초안 (`.txt`, 리더가 옮김)

- `draft_1_REQ-DISP_polarity_and_auto_window.txt` — 새 요구 둘(극성, 자동 창)과 REQ-DISP-019 개정 한 줄. 상수·한계·검증 시험·반증을 붙였다.
- `draft_2_REQ-DISP-017_revision.txt` — 창의 도메인을 "VOI 에 도달하는 값의 도메인" 으로, 프리셋과 자동 창의 관계(REQ-DISP-017a), `#151` 인용 확인 필요, 프리셋 `maxOut 255` 와 Presentation 입력 [0,1] 의 연결.
- `draft_3_SRS-DICOM_polarity_sentence.txt` — SRS-FUNC-023 의 극성 문장 교체안, SDD 극성 문단과 MONOCHROME1 Edge Case 행, 열린 점 3개(자료의 극성 정보가 따라다니지 않음, 자동 창의 배경 방향 가정, 다른 레인 영향).

## 5절 요약 (verification-claim-integrity §3)

**Claim**: 위 결론 표. **Evidence**: 이 폴더의 `p1_chain.txt`, `p2_times_run*.txt`, `p2_presentation_ab_run*.txt`, `p6_leak.txt`, `p8_variants.txt`, `arms_falsification.txt`, `display_tests_results.txt`, `e2e_default_tests.txt`, `doxygen_err.txt`(빈 파일).
**Baseline-attribution**: 제품 `b796ba00` + 하니스(다음 커밋), `ci-post`(RelWithDebInfo), 입력 `d152e8eb…fdfd4`. 비교 기준은 QA-B-213(`ecb00ac8`)의 같은 시험.
**Gaps (미검증)**:
- 영상은 손목 측면 한 장이다. 모든 상수(특히 0.05, 0.005)와 오츠의 이분성 가정은 한 장의 해부 영상과 평탄장 몇 장에서 정했다. 다른 부위(두 조직 집단, 프레임을 채운 손, 콜리메이터 그림자)에서는 측정하지 않았다.
- 실제 MONOCHROME1 파일로 읽기→체인→표시를 돌려 두 번 뒤집기를 확인하지 못했다. 코드 인용과 산술로만 적었다.
- 다른 레인의 호출처(gui, clients)는 읽기만 했고 실행하지 않았다. 그쪽 시험이 빨개지는지는 모른다.
- 내보낸 함수 개수를 단언하는 문서·시험이 display 시험 바깥에 있는지 확인하지 않았다.
- PS3.3 C.11.6 (INVERSE 형태)은 표준 원문을 이 세션에서 다시 열지 않았다.
- P2 의 부하 영향은 직접 측정하지 않았다(CLAHE·노이즈 같은 코드 무변경 단계의 변동으로 추정). Presentation 만 A/B 로 확인했다.
- 육안 평가는 축소 미리보기로 한 저자 관찰이다.
**Residual-risk**: 5% 분위수·0.75 분리도 같은 값은 두 번째 부위에서 다시 정해야 할 수 있다. 기본 극성이 바뀌어 외부 호출처가 말없이 뒤집힌 영상을 받는다는 점은 다른 레인과의 합의가 필요하다.
