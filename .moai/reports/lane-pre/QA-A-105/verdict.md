# QA-A-105 — 교정 적재 C·D·E, 화소 간격 140 µm, #187 판독 (#179, #187)

커밋 `e0b19fb` (dev/preprocess, 미푸시, 기준 main `c72f0e8`)

## 1. 주장

1. **C(읽으면서 해시)**: 페이로드를 1 MiB 단위로 읽으며 SHA-256 을 갱신한다. 파일 순서대로 먹이므로 다이제스트는 SHA-256(config ‖ payload) 그대로다.
2. **E(0 초기화 제거)**: 교정 맵 버퍼를 `make_unique`(값 초기화) 대신 `new[]`(기본 초기화)로 잡는다. 바로 뒤 `memcpy` 가 전부 덮어쓴다.
3. **D(사용 방식 명시)**: `xpe_preprocess_pipeline` 헤더에 프레임마다 3파일을 다시 읽는다는 경고와 권장 경로(`xpe_calib_state_load` 1회 + `pipeline_ex`)를 적었다. 함수는 그대로 둔다. `pipeline.cpp` 에 `@MX:WARN` 을 붙였다.
4. 적재 3파일 합계 중앙값은 약 549 ms → 475 ms 다. **요구 200 ms 는 여전히 넘는다**(2.4배).
5. 적재 결과는 바뀌지 않는다. 수정 전후에 적재한 맵을 파일로 다시 써서 페이로드를 바이트 비교했다.
6. 전처리 소유 코드에서 화소 간격 값이 쓰이는 곳은 시험 한 곳뿐이었고 0.143 → 0.14 로 바꿨다. 다른 값은 post 레인 소유다.
7. #187: 다항식 게인 파일을 적재하면 게인 보정이 `XPE_ERR_CALIB_NOT_LOADED` 로 실패한다. 현재 동작을 시험 3건으로 고정했다(고치지 않음).

## 2. 증거

### 2.1 적재 시간 (3072², 준비 3회 제외, 중앙값)
같은 측정 프로그램(`lbench.cpp`)으로 DLL 만 바꿔 실행했다. 각 조건 3회.

| | 수정 전 (N=15, 25, 25) | 수정 후 (N=15, 25, 25) |
|---|---|---|
| offset 37.7 MB | 210.6 / 233.3 / 233.0 | 201.9 / 193.2 / 207.4 |
| gain 37.7 MB | 240.4 / 256.6 / 252.5 | 208.5 / 209.3 / 228.6 |
| defect 9.4 MB | 58.8 / 58.7 / 67.3 | 62.8 / 61.5 / 64.9 |
| 3파일 합계(중앙 실행 기준) | 약 549 | 약 475 |

- 부분별(수정 전 코드와 같은 순서로 재현한 값, `load-breakdown.log`): offset 기준 페이로드 읽기 32.9, SHA-256 169.7–190.5, 할당+복사 10.3–13.4, 열기+헤더+JSON 0.15 ms.
- SHA-256 자체는 줄지 않는다(같은 계산량). 줄어든 것은 읽기와 해시가 겹치는 부분과 0 초기화다.
- 측정 중 다른 레인 부하: 측정 전 CPU 17–20%, GUI 레인 `dotnet` 실행 중. 컴파일러·링커 없음.

### 2.2 출력 동일성
- 적재 후 `xpe_calib_save` 로 다시 쓴 파일을 수정 전후로 비교했다.
- 헤더의 `created_epoch_ms` 는 실행마다 다른 원본 파일에서 오므로 파일 전체는 다르다. **페이로드(152바이트 헤더 이후)는 offset·gain·defect 모두 동일**하다(`cmp -i 152`). 앞 24바이트(매직·버전·형식·크기)도 동일하다.
- 전처리 `ctest` 681건 통과, 종료 0(`a105-ctest.log`). `BUILD_EXIT=0`, warning C 0.

### 2.3 남은 간격과 선택지 A
- 요구 200 ms, 현재 약 475 ms → 약 275 ms 초과(2.4배).
- 남은 시간의 대부분은 여전히 SHA-256(3파일 합계 약 415 ms, PicoSHA2 약 190 MB/s)이다.
- 선택지 A(플랫폼 해시 구현: Windows CNG `BCryptHash`, 또는 SHA-NI 사용 구현)
  - i7-12700 은 SHA 확장을 지원하지 않는다(Intel 은 Ice Lake 서버·Alder Lake 이후 세대별로 다르며, 이 CPU 에서는 확인하지 않았다 — **미검증**).
  - 기대 효과를 재지 않았다. 해시가 2배 빨라지면 3파일 합계는 약 265 ms, 4배면 약 165 ms 가 된다(산술 추정).
  - 다이제스트가 같으므로 파일 호환은 유지된다.
- 이 카드에서는 여기서 멈춘다(카드 지시).

### 2.4 화소 간격

| 위치 | 값 | 조치 |
|---|---|---|
| `modules/preprocess/tests/test_xpe_preprocess_correction.cpp:71` | 0.143 → **0.14** | 바꿨다 |
| `modules/preprocess/src/preprocess.cpp:25` | 유효 범위 0.1–0.5 | 0.14 를 포함한다. 그대로 |
| `modules/common/include/xpe/common/xpe_types.h:113` | 필드 정의(값 없음) | 그대로 |
| `modules/dicom/tests/*` (4곳) | 0.148, 0.200 | post 레인 소유 — 보고만 |
| `modules/enhance_advanced/tests/*` (3곳) | 0.139, 0.2 | post 레인 소유 — 보고만 |
| `modules/enhance_basic/tests/*` (2곳) | 0.148, 0.139 | post 레인 소유 — 보고만 |
| `docs/calibration/SAD-CALIB-001…md:325` | 143.5 µm | lead 소유 — 보고만 |
| `docs/calibration/xray-detector-calibration-prd.md:1717, 2350` | 150.0 µm, 범위 50–500 | lead 소유 — 보고만 |
| `tools/mcsim/README.md:242` | `pixelPitchMm = 4.0` | 검출기 화소가 아니라 몬테카를로 영상의 굵은 화소 크기다. 해당 없음 |

- 전처리에는 화소 간격을 소스에 박아 둔 곳이 없다. 값은 `XpeImageMetadata::pixelPitch_mm` 으로 들어온다(DICOM 판독 또는 호출자). 설정·메타데이터 경로가 이미 있어 그대로 쓴다.

### 2.5 #187 판독
- 요구 원문
  - FUNC-027(`SRS-CALIB-001:157`): "For each pixel (x,y), fit polynomial `G(x,y,E) = Σ(c_k × E^k)` … Output: polynomial coefficient array stored in `.xpe_calib` with `XCAL_TYPE_GAIN_POLY`." **적용 시점·방법은 적혀 있지 않다.**
  - FUNC-005(`:41`): "System shall apply gain (flat-field) correction: `I_norm(x,y) = I_corr(x,y) / G(x,y)` … Multi-gain mode with energy-dependent polynomial `G(x,y,E) = Σ(c_k × E^k)` shall be supported for SID-specific gain maps." 적용은 여기 있고, 어떤 API 인지는 적혀 있지 않다.
- 현재 동작(코드)
  - `xpe_calib_load_gain.cpp:76-90`: GAIN_POLY 를 받으면 계수를 저장하고 스칼라 맵을 지운다.
  - `gain_correct.cpp:275`: 스칼라 맵만 읽고, 없으면 `XPE_ERR_CALIB_NOT_LOADED`.
- 시험 3건(`test_gain_poly_not_applied.cpp`), 모두 통과
  - 대조: 스칼라 맵이면 1000/2 = 500 이 나온다.
  - 다항식 파일은 적재는 되고 보정은 `XPE_ERR_CALIB_NOT_LOADED` 다. 출력 버퍼는 건드려지지 않는다.
  - 스칼라 맵이 있던 상태에서 다항식을 적재하면 동작하던 보정이 실패로 바뀐다.
- 선량(E)을 어디서 받을지 선택지

| 선택지 | 내용 | 영향 |
|---|---|---|
| 1. `XpeImageMetadata` 에 선량/에너지 필드 추가 | 구조체에 `doseE` 류 필드 추가 | 구조체 크기가 바뀐다. `static_assert(sizeof(XpeImageMetadata) == 96)` 와 C# P/Invoke 를 함께 고쳐야 한다. 공개 ABI 변경 |
| 2. 기존 필드에서 유도 | `kVp`(있음)로 E 를 잡는다 | ABI 변경 없음. FUNC-027 근거 열이 "multiple kVp values (50-120 kVp)" 를 말하므로 의미가 맞다. 다만 교정 때 쓴 `dose_levels` 가 kVp 인지 선량인지 코드로 확인되지 않는다(생성 함수는 단위를 규정하지 않는다: "dose levels (mGy or relative units)") |
| 3. 설정 JSON 으로 받음 | `xpe_gain_correct` 에는 설정 인자가 없다. 파이프라인 설정에 넣고 내부 함수로 전달 | 공개 ABI 변경 없음. 프레임마다 값이 달라지면 설정을 매번 바꿔야 한다 |
| 4. 새 공개 함수 | `xpe_gain_correct_poly(input, output, meta, E)` | 기존 호출자 영향 없음. 내보내기 심벌이 늘어난다(REQ-P0-008 개수 제약 확인 필요) |

- 단위 문제는 어느 선택지든 먼저 정리해야 한다. 생성 함수는 `dose_levels` 를 "mGy or relative units" 로만 적고, 적용 쪽 요구는 E(에너지)로 적는다.

## 3. 기준 귀속
- 기기·빌드는 QA-A-102~104 와 같다(i7-12700, `build/ci-preprocess` RelWithDebInfo).
- 수정 전 값은 같은 트리에서 C·E 변경분만 되돌린 빌드(보관 후 복원)로 측정했다.

## 4. 미검증
- 선택지 A 의 실제 효과(플랫폼 해시, SHA-NI 지원 여부)를 재지 않았다.
- 1 MiB 청크 크기를 다른 값과 비교하지 않았다.
- 압축된 XCal(RLE) 경로는 이 측정에 포함되지 않았다(시험은 통과).
- post·lead 소유 파일의 화소 간격 값은 바꾸지 않았고, 그 값들이 어떤 시험 결과를 바꾸는지도 확인하지 않았다.
- #187 은 판독만 했다. 선택지의 ABI 영향(구조체 크기, 심벌 개수)은 코드로 확인하지 않았다.

## 5. 잔여 위험
- 적재는 요구의 2.4배다. 시작 시 1회라면 실사용 영향은 제한적이지만, 요구는 그대로 미달이다.
- `new[]` 기본 초기화는 `memcpy` 로 전부 덮어쓴다는 전제에 기댄다. 부분 복사로 바뀌면 초기화되지 않은 값이 남는다. 현재 코드는 전체를 복사한다(`payload.size()` 전체).
- 스트리밍 해시는 청크 경계에서 `f.gcount()` 로 길이를 확인한다. 짧은 읽기는 `XPE_ERR_IO_FAILED` 가 된다(기존과 같음).
