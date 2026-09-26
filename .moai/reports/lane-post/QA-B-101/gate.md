# QA-B-101 게이트 — 적용 여부 신호, 제품용 표, 화소 간격 (#180)

커밋 `122108e`, 미푸시.

## 1. 주장

### §1 적용 여부 신호 (ABI 추가만)

새 진입점을 추가했다.

```
xpe_gsvg_process_ex(... process_masked 인자 ..., XpeGsvgResult* resultOut)
```

**구조체 `XpeGsvgResult`**
- 크기 24 바이트이고 패딩이 없다. 모든 멤버가 32비트다.
- 멤버: `structSize`(입력), `vignetteApplied`, `gridSuppressed`, `virtualGridApplied`, `restoredOriginal`, `reason`.

**사유 `XpeGsvgReason`**(값 고정)

| 값 | 이름 | 뜻 |
|---|---|---|
| 0 | APPLIED | 설정된 단계가 영상을 바꿨다 |
| 1 | NOT_CONFIGURED | 억제·가상 그리드 둘 다 꺼져 있다 |
| 2 | IMAGE_TOO_SMALL | 32 px 미만 |
| 3 | NO_GRID_DETECTED | 입력 스펙트럼 게이트에서 격자 피크가 없다 |
| 4 | GRID_NOT_IN_SUBBANDS | 입력 피크는 있으나 서브밴드 검출이 없어 필터를 적용하지 않았다 |
| 5 | VG_REFUSED | 가상 그리드가 거부했다. dst 는 원본이다 |

**판정 규칙**
- `resultOut` 이 NULL 이거나 `structSize < 24` 이면 `XPE_ERR_INVALID_INPUT` 을 돌려준다. 이 검사는 처리보다 먼저 한다.
- 결과는 `XPE_OK` 와 가상 그리드의 `XPE_ERR_CONFIG_INVALID` 에서 채운다.
- 그리드 억제의 "적용"은 `SuppressGrid` 보고서에 band-stop 결정(`decisions`)이 하나 이상 있다는 뜻이다.
- 가상 그리드가 거부되어 원본으로 되돌리면 비네팅도 되돌아가므로 `vignetteApplied = 0` 이다.
- 기존 `xpe_gsvg_process` / `_masked` 는 내부에 NULL 결과를 넘길 뿐, 동작이 같다. 기존 gsvg 시험은 기대값을 바꾸지 않고 통과했다.

### §2 제품용 표

**로더 확장** (`virtual_grid.cpp` [grid])
- `freq_per_cm`, `thickness_cm`, `kvp` 열을 선택적으로 받는다.
- 행을 그대로 보관하고, (격자비, 선밀도) 설계마다 두께 × kVp 격자가 완전한지 검사한다. 중복과 0 이하 값은 거부한다.
- 처리할 때 `SelectGrid` 가 격자비와 선밀도로 설계를 고르고 kVp 로 선형 보간한다. 표 밖 kVp 는 거부한다.
- Ts/Tp 는 축소 격자 화소마다 추정 두께로 선형 보간한다(`GridAtKvp::ResidualAt`). 표의 두께 범위(10–30 cm) 밖은 끝값을 쓴다. 산란과 같은 방식으로 원해상도로 올린다.
- 선택 열이 없는 옛 형식(격자비당 한 행)은 전과 같이 읽고 결과도 같다. 합성 표·MC 시험이 기대값 변경 없이 통과했다.

**설정 키** `vg_grid_frequency_per_cm`
- 표에 선밀도 열이 있으면 필수다.
- 열이 없는데 주면 거부한다.
- 0 이하면 init 에서 거부한다.
- 선밀도 누락은 kVp 표 밖과 같이 **process 시점**에 VG_REFUSED 로 판정된다.

**제품용 표** `modules/gsvg/data/vg_table_water_csi600_victre.csv`
- `modules/gsvg/tools/make_vg_table.py` 가 main 의 `tools/mcsim/tables` 세 파일을 줄 단위로 복사해 합친다. 값은 계산하지 않는다.
- 원본의 git blob 해시를 표 머리에 적었다. `git hash-object` 결과와 같다.
  - kernels `2c61706b…`
  - wet `ecdac143…`
  - grid `55633f2f…`
- 각 원본 머리말의 "simulation-based, not calibrated" 줄과 파일 머리의 "NOT CALIBRATED" 를 유지했다.
- `[spr_cap]` 은 없다. 상한은 커널 합이다.
- 내용: 커널 gauss4 6 두께 × 7 kVp, wet 7 kVp, grid 96행 = 격자비 6/8/10/12 × 40·60 /cm × 10/20/30 cm × 60/80/100/120 kVp.

### §2 대표 행 방식과의 비교 (리더 결정 사항)

`_residual_spread.log` 은 grid_water_victre.csv 에서 계산했다. 가상 그리드가 쓰는 값은 Ts/Tp 다.

| 격자비 | 선밀도 | 최소 | 최대 | 최대/최소 | 대표(20 cm, 80 kVp) | 대표 대비 최대 오차 |
|---|---|---|---|---|---|---|
| 6 | 40 | 0.1590 | 0.5127 | 3.23 | 0.2695 | 90% |
| 6 | 60 | 0.2007 | 0.6467 | 3.22 | 0.3762 | 72% |
| 8 | 40 | 0.1122 | 0.3981 | 3.55 | 0.1828 | 118% |
| 8 | 60 | 0.1354 | 0.5378 | 3.97 | 0.2674 | 101% |
| 10 | 40 | 0.0857 | 0.3102 | 3.62 | 0.1302 | 138% |
| 10 | 60 | 0.0982 | 0.4440 | 4.52 | 0.1935 | 129% |
| 12 | 40 | 0.0693 | 0.2431 | 3.51 | 0.0976 | 149% |
| 12 | 60 | 0.0761 | 0.3657 | 4.81 | 0.1434 | 155% |

- 대표 행을 고르려면 선밀도 하나와 (두께, kVp) 한 점을 정해야 한다. 표 범위 안에서 Ts/Tp 는 3.2–4.8 배 변한다.
- 구현은 로더 확장으로 했다. 대표 행 방식도 이 로더로 표현할 수 있다. 선택 열을 빼고 격자비당 한 행만 두면 된다.
- 선택은 리더가 한다.

**파일 위치·배포 (제안)**
- 저장소 안에서 CMake `install()` 규칙은 찾지 못했다(`grep install` 결과: `cmake/XpeCoverage.cmake` 의 안내문뿐).
- 제안: 설치물에서 `gsvg.dll` 옆 `data/gsvg/` 에 이 파일을 넣고, 호출자는 그 경로를 `vg_table_path` 로 넘긴다.
- 이 카드에서는 배포 규칙을 추가하지 않았다(`cmake/` 는 리더 소유).
- 시험은 작업 디렉터리 `modules/gsvg` 기준 `data/…` 경로로 읽는다.

### §3 화소 간격 (근거만)

| 값 | 위치 | 근거 |
|---|---|---|
| 0.139 | gsvg 헤더 예시(`gsvg_api.h:84`), 시험·벤치(`grid_test_tools.h:93`, `test_grid_suppression.cpp:30`, `test_grid_tools.cpp:26`) | 저장소 문서 중 유일한 출처는 `docs/project/xpe-implementation-reference.md` 옛 판의 `pixelPitch_mm` 기본값 `0.139f (139 μm)`, 출처는 DICOM (0018,1164) Imager Pixel Spacing 이었다. 이 줄은 커밋 4212f76(2026-04-15)에서 삭제되었고, 현재 판(146행)은 "detector configuration or DICOM, required" 로 기본값이 없다. 이후 gsvg 코드에는 b5bf1d5/361ee27/5cf924f(QA-B-89~91)에서 시험 값으로 들어왔다. |
| 0.14 | clients 시험(`DataSizeContractTests.cs:219`, `PreprocessCorrectionChainSmokeTests.cs:295`) | 읽기만 했다(Lane C 소유) |
| 0.143 | clients 서비스 4곳(`NativeEnhanceBasicPreviewService.cs:367` 등) | 읽기만 했다 |
| 143.5 μm | `docs/calibration/SAD-CALIB-001…:325` 주석 | — |
| 0.1 | gsvg 문서(`IAP-GSVG-001:479`, `TDS-GSVG-001:124`, `README.md:710`) | 합성 데이터 예시 |

- gsvg 는 화소 간격에 기본값이 없다. `vg_pixel_pitch_mm` 은 필수이고, 격자 억제는 화소 간격을 받지 않는다(주파수를 cycles/pixel 로 검출).
- 가상 그리드에서 화소 간격이 쓰이는 곳은 두 군데다: 커널 반경의 cm 환산, 축소 격자 배율(`virtual_grid.cpp` `pitchCm`).
- 0.139 는 gsvg 시험의 편의값이며, 현재 저장소 문서에서 제품 값으로 뒷받침되지 않는다.

## 2. 증거

- `_run1.log`: `GsvgResult.*`, `GsvgAbiExports.*` 8건 통과(BUILD=0). 처음 빌드는 `Run` 이름이 `testing::Test::Run` 과 부딪혀 실패했고, `Outcome` 으로 바꿨다.
- `_run2.log`: gsvg_tests 전체 129건 통과(BUILD=0).
- `_falsify_G1.log`: `ResidualAt` 이 두께를 무시하도록 바꾸자 `GsvgVgProductTable.ResidualFollowsThickness` 가 실패했다(BUILD=0). 복원본은 `_vg_ok.cpp.txt`, `FALSIFY` 문자열 0건.
- `_verify.log`
  - ci-post: BUILD=0, 633 통과. 이전 619 대비 +14 = GsvgResult 7 + GsvgVgProductTable 7.
  - ci-ai-b20: BUILD=0, 225 통과.
  - ci-dicom: BUILD=0, 194 통과.
  - e2e: 28 통과.
- `_residual_spread.log`: §2 비교표.

## 3. 기준선 귀속

- 모든 수치는 이 워크트리 `dev/postprocess`(HEAD 122108e 와 같은 소스)의 이번 빌드에서 잰 값이다.
- mcsim 원본은 main 체크아웃(`D:/workspace-github/image-processing`, `tools/mcsim/tables` 최종 커밋 9fd0ec6)에서 읽었다.

## 4. 미검증

- `GRID_NOT_IN_SUBBANDS`(사유 4)를 만드는 영상을 찾지 못했다. 이 사유를 내는 시험은 없다. 판정 코드만 있다.
- §1 의 반증은 따로 돌리지 않았다. 사유 기대값이 기본값(NOT_CONFIGURED)과 다른 것으로 구별력을 삼았다.
- 제품용 표로 MC 팬텀을 처리한 정확도는 재지 않았다. 표 자체가 미보정이다.
- 선밀도를 init 에서 검사하지 않는다. 표를 읽는 시점과 설정의 교차 검사가 process 시점이다.
- 실제 설치·배포는 시험하지 않았다.
- 생성 스크립트를 CI 에서 다시 돌려 파일과 비교하는 시험은 없다. 이 워크트리에는 mcsim 표가 없다.

## 5. 잔여 위험

- `XpeGsvgResult` 에 멤버를 더하면 `structSize` 검사가 "최소 24" 로 남아야 한다. 지금 코드는 `sizeof` 로 비교하므로, 확장할 때 최소 크기 상수로 바꿔야 한다.
- 두께 범위(10–30 cm) 밖은 끝값 고정이라 5 cm 같은 얇은 부위의 Ts/Tp 는 표 밖 추정이다.
- `.csv` 줄바꿈: 로더는 CRLF 를 처리한다(QA-B-97). 생성 스크립트는 LF 로 쓴다.

## Card Cross-Check

| 카드 항목 | 결과 |
|---|---|
| §1 신호, 구조체 크기·오프셋 시험 | 122108e, `GsvgResult.LayoutIsFixed` |
| §1 격자 없음 / 있음 / VG 설정 틀림 | NO_GRID_DETECTED / APPLIED / VG_REFUSED + 복원 |
| §2 한 파일 제품용 표, 로더 확장 | `data/vg_table_water_csi600_victre.csv`, `SelectGrid` |
| §2 대표 행 비교·근거 | §1 비교표 (결정은 리더) |
| §2 위치·배포, 미보정 표시 | 제안만, 표시 유지 |
| §2 시험: 로드, 격자비 6/8/10/12 | `LoadsWithEveryRatioOfReqGsvg016` |
| §3 화소 간격 근거 | §1 표 |
