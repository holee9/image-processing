# tools/calibration — 두 엔진 출력 비교 계약

`comparison_manifest.py`는 2G(삼성 SDSDK 2.3.0 B03 X64W)와 새 전처리 엔진(PRE)이 이미 만든
출력 파일 두 개를 manifest 하나로 묶어, 비교해도 되는지 검사하고 숫자 차이를 계산한다.
운영자 앱의 비교 창이 이 스크립트를 하위 프로세스로 실행한다.

- 앱에서 쓰는 방법: [docs/calibration/HOWTO-Compare-2G-PRE.md](../../docs/calibration/HOWTO-Compare-2G-PRE.md)
- 관련 이슈: [#278](https://github.com/holee9/image-processing/issues/278), [#285](https://github.com/holee9/image-processing/issues/285)

## 하는 일과 하지 않는 일

| 한다 | 하지 않는다 |
|---|---|
| 이미 있는 파일 읽기, 해시 확인 | 엔진 실행, 2G DLL 불러오기 |
| 비교 조건 검사와 사유 코드 반환 | 영상 품질 판정, "더 좋다" 판단 |
| A − B 숫자 차이 통계 계산 | 빠진 값 추정, 정의되지 않은 필드 무시 |

## 명령

```text
python comparison_manifest.py validate <manifest.json> [--base-dir <dir>]
python comparison_manifest.py compare  <manifest.json> --out <runs-dir> [--base-dir <dir>]
```

`--base-dir`를 생략하면 manifest가 있는 폴더를 기준으로 상대 경로를 푼다. 스크립트는 그 폴더 밖의
파일을 읽지 않는다(`PATH_OUTSIDE_BASE`).

## manifest

- 스키마 이름: `xpe-2g-pre-comparison/4`
- 최상위 키: `schema`, `input`, `engines`, `roi`, `comparison_kind`, `calibration_burden`, `note`,
  `dtype_promotion`. 이 밖의 키는 거부한다(`UNSUPPORTED_FIELD`).
- 엔진은 정확히 둘이다. `A` = `samsung-2g-x64w`, `B` = `new-pre`.
- 자료형: `uint16`, `int16`, `uint32`, `int32`, `float32`(리틀 엔디언). 두 쪽이 같아야 한다.
  정수 대 `float32`는 `dtype_promotion: "float64-lossless"`를 적으면 둘 다 float64로 올려
  비교한다. 이 변환은 값을 잃지 않는다.
- 배치는 왼쪽 위 원점·위에서 아래·왼쪽에서 오른쪽·값 변환 없음 하나만 지원한다.
- 빠진 값, `unknown` 같은 값은 양쪽이 같아도 "같음"으로 보지 않고 사유로 남긴다.

## 비교 종류

`comparison_kind`는 아래 셋 중 하나다. 사유가 하나라도 허용 목록 밖이면 `comparable=false`다.

| 종류 | 허용하는 사유 | 알고리즘 차이로 해석 |
|---|---|---|
| `common-condition` (기본) | 없음 | 가능 |
| `e2e-non-isolated` | `MAP_IDENTITY_DIFFERS`, `SETTINGS_DIFFER`, `SETTINGS_UNVERIFIED`, `STAGE_NOT_ATTRIBUTABLE` | 불가 |
| `stored-array-difference` | 위 넷과 `LAYOUT_UNVERIFIED` | 불가 |

`algorithm_attribution=true`도 품질이 더 좋다는 뜻은 아니다.

## 사유 코드

| 묶음 | 코드 |
|---|---|
| 파일 | `MISSING_FILE`, `SHA_MISMATCH`, `FILE_SIZE_MISMATCH`, `PATH_OUTSIDE_BASE`, `SOURCE_CHANGED_DURING_RUN` |
| 형식 | `SCHEMA_MISMATCH`, `UNSUPPORTED_FIELD`, `UNSUPPORTED_DTYPE`, `DTYPE_MISMATCH`, `UNSUPPORTED_DTYPE_PROMOTION`, `UNSUPPORTED_ENDIAN`, `UNSUPPORTED_STRIDE`, `UNSUPPORTED_LAYOUT`, `LAYOUT_UNVERIFIED`, `GEOMETRY_MISMATCH`, `UNITS_MISMATCH`, `UNSUPPORTED_UNITS` |
| 입력·조건 | `INPUT_LINK_MISMATCH`, `CORRECTION_STATE_UNVERIFIED`, `MODE_MISSING`, `MODE_MISMATCH`, `SETTINGS_DIFFER`, `SETTINGS_UNVERIFIED`, `MAPS_UNDECLARED`, `MAP_IDENTITY_DIFFERS` |
| 엔진·단계 | `ENGINE_IDENTITY_MISMATCH`, `ENGINE_OUT_OF_SCOPE`, `UNSUPPORTED_STAGE`, `STAGE_MISMATCH`, `STAGE_NOT_ATTRIBUTABLE` |
| 영역·값 | `INVALID_ROI`, `NONFINITE_VALUES` |
| 실행 | `VENDOR_LIVE_BLOCKED`, `LIVE_EXECUTION_NOT_AUTHORIZED`, `UNKNOWN_COMPARISON_KIND`, `EXCEPTION` |

결함 화소 맵은 바이트가 같은 파일일 때만 같은 맵으로 본다.

## 시험

```text
cd tools/calibration
python -m unittest test_comparison_manifest
```

2026-11-18 실행 결과: `Ran 58 tests`, `OK`. 이 시험은 아직 CI 워크플로에 들어 있지 않다.
