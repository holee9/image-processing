# QA-B-181g — Codex #57 보류 2건: 빠진 생산자, J2K 표 상한 위반 분류 (#233 #235)

Refs #233 #235

## 0. 먼저 읽을 것 (리더가 판단할 것 포함)

| 항목 | 결과 |
|------|------|
| Codex 지적은 맞았다 | 181f 의 "noise_reduce 측정했고 없음"은 작은 시그마 몇 값만 본 것이었다. ±FLT_MAX 입력에서 `noise_reduce`(bilateral·NLM)와 `edge_enhance` 가 rc=0 으로 비유한 영상을 냈다. |
| **Codex 가 못 본 생산자 하나가 더 있었다** | 전수 훑기에서 **`xpe_multiscale_process` 가 5/5 모두 비유한 출력(rc=0)**. 같이 고쳤다. |
| 방식 — **리더 결정이 필요** | 카드는 "double 누산으로 넘침 자체를 없앨 수 있으면 우선, 정상 영상 출력이 바뀌면 차이 크기를 보고하고 멈춤"이었다. **double 누산은 택하지 않았다**: float→double 누산은 정상 영상의 반올림도 바꾸므로 모든 영상의 비트가 달라진다(구성상 확실, 차이 크기는 **측정하지 않았다**). 대신 **첫 쓰기 전에 거부**하는 방식으로 정상 영상은 **비트 동일**하다(§4). 그 대가로 ±FLT_MAX 급 영상은 계산 가능한 것도 거부한다. double 누산으로 가면 flat FLT_MAX 같은 영상도 정확히 계산되지만 비트가 바뀐다. |
| J2K 표 상한 | 맞는 지적이었고 제 182e 분류가 표의 `Bits Stored 1–38` 을 보지 않았다. 고쳤다. |
| 비용 | bilateral +3.7 ms(+3.7%), edge_enhance +1.8 ms(+10.9%, 18.5 ms 로 모듈의 20 ms 예산 안), multiscale·NLM 은 오차 안 (§4). |

## 1. 최악 입력 전수 훑기 (카드 "생산자 전수를 다시")

`worst_input_sweep_before_after.txt`. 이미지를 돌려주는 공개 함수 11개를 **±FLT_MAX 5가지 패턴**(전부 +MAX, 전부 −MAX, 체커보드 ±MAX, 0 속의 MAX 한 화소, −MAX..MAX 의 유한한 램프) × **허용 파라미터의 극단**으로 돌렸다. **결함 = rc 가 OK 인데 출력에 비유한 값이 있음**. 입력 자체가 유한한지는 하니스가 단언한다 (처음 버전은 램프를 float 로 계산해 `2.0f*FLT_MAX = +inf` 가 입력에 섞였고, 그 값으로 "결함"을 보고했다 — 하니스를 고치고 전부 다시 돌렸다).

"수정 전" = HEAD(`b69d22df`) 소스로 다시 빌드한 DLL, "수정 후" = 이 카드의 소스.

| 공개 함수 | 케이스 | 수정 전 결함 | 수정 후 결함 (rc==OK 건수) | "없음" 의 근거 |
|-----------|--------|--------------|----------------------------|-----------------|
| `xpe_noise_reduce` bilateral | 100 | **56** | 0 (0) | 최악 입력 훑기 |
| `xpe_noise_reduce` NLM | 75 | **30** | 0 (0) | 〃 |
| `xpe_edge_enhance` | 300 | **20** | 0 (75, 모두 amount=0 의 무동작) | 〃 |
| `xpe_multiscale_process` | 5 | **5** | 0 (0) | 〃 |
| `xpe_contrast_enhance` | 60 | 0 | 0 (36) | 최악 입력 훑기 (181f 의 범위 오버플로 검사가 24건을 거부) |
| `xpe_log_transform` | 25 | 0 | 0 (21) | 〃 |
| `xpe_log_inverse` | 25 | 0 | 0 (9) | 〃 |
| `xpe_apply_modality_lut` LINEAR | 75 | 0 | 0 (23) | 〃 |
| `xpe_apply_modality_lut` TABLE | 5 | 0 | 0 (5) | 〃 |
| `xpe_apply_voi_lut` (3 모드) | 540 | 0 | 0 (540) | 〃 — 중심·폭·출력 범위의 ±FLT_MAX·1e-30 극단 포함 |
| `xpe_fractional_process` | 25 | 0 | 0 (25) | **설계상**: 비유한 입력을 0 으로 바꾸고(SPEC REQ-ADV-032) 출력이 클램프됨. 입력이 유한한 ±FLT_MAX 일 때도 출력이 유한함을 측정으로 확인 |

훑지 않은 것 (근거를 다르게 적는다): `xpe_apply_presentation_lut` (출력이 uint16 으로 클램프, 181f 에서 입력 검사), `xpe_gsvg_process` (입력 uint16 이라 ±FLT_MAX 가 없음 — 설정 극단은 181f 에서 `vg_pyramid_gain` 1e308 로 측정), `xpe_detect_collimation` (출력이 정수 좌표), `xpe_noise_estimate_sigma`·`xpe_calc_exposure_index` 등 스칼라를 돌려주는 함수 (영상 출력이 아니다).

훑기의 한계: 32×32 (NLM 24×24, 다중 스케일 64×64) 작은 영상, 5개 패턴, 열거한 파라미터 값만. 다른 크기·패턴·파라미터에서 새 결함이 나올 가능성은 배제하지 못한다.

## 2. 수정 — 방식과 근거

모두 `XPE_ERR_INVALID_INPUT`, 입력은 한 바이트도 바뀌지 않는다.

| 함수 | 방식 | 왜 이 방식인가 |
|------|------|----------------|
| `xpe_noise_reduce` bilateral | 첫 쓰기 전에 입력 한 번 훑어 최댓값 M 을 구하고, `M × ksize × 1.01 > FLT_MAX` 이면 거부. 비유한 화소도 같은 훑기에서 거부 | 제자리 쓰기라 사후 검사로는 "쓰기 전 거부"를 못 한다. 가중치는 모두 ≤1 이라 한 번의 합은 ≤ ksize·M. 가중 평균의 정확한 결과는 늘 M 이하라 표현 가능하지만, **float 합**이 넘칠 수 있다. |
| `xpe_noise_reduce` NLM | 같은 방식, 상한은 `M × search_window² × 1.01` | 한 화소의 가중치 합은 탭 수 이하 |
| `xpe_edge_enhance` | `M × 12 × 1.01 > FLT_MAX` 이면 거부 | 정규화된 블러 ≤ M(1+반올림), diff ≤ 2M, `amount × diff` ≤ 5×2M, sharpened ≤ 11M. 한계가 ±inf 로 넘치면 선택되지 않는다. 12M 은 그 상한 + 여유 |
| `xpe_multiscale_process` | 결과를 만든 `reconstructed` 버퍼가 유한한지 **복사 직전에** 검사 | 결과를 별도 버퍼에 만들었다가 마지막에 복사하는 구조라 **정확한** 사후 검사가 가능하다. 결과가 실제로 비유한일 때만 거부하고 NaN 입력도 거기서 걸린다. |
| `xpe_noise_reduce` NLM `h_param` | `h_param` 의 유한성 비트 검사 추가 | `h_param <= 0` 은 +inf 를 어느 모드에서나, NaN 을 `/fp:precise` 에서 통과시켜 `h2_inv = 0` 이 된다 (181e 와 같은 부류) |

**검사 위치의 교훈**: 처음에는 스캔을 맨 앞에 두었다가 기존 시험 2건(QA-B-181 의 "크기가 불가능한 이미지는 `OUT_OF_MEMORY`")이 깨졌다. 그 시험들은 거짓 크기의 이미지에서 **할당이 먼저 실패**하는 것에 의존하는데 내 스캔이 할당보다 앞서 화소를 읽어서 순서를 바꿨다. 스캔을 할당 뒤, 첫 쓰기 앞으로 옮겼다. 이 순서는 코드 주석에 이유와 함께 남겼다.

선택하지 않은 방식:
- **double 누산**: 위 §0. 넘침 자체를 없애 flat FLT_MAX 도 계산하지만 정상 영상의 비트를 모두 바꾼다. 리더 결정 사항.
- **2의 거듭제곱으로 입력·파라미터를 미리 줄였다가 되돌리기**: 정상 영상은 비트 동일하게 둔 채 극단 영상도 계산할 수 있지만 함수마다 별도 구현이 필요하고 되돌릴 때 다시 넘칠 수 있다. 시도하지 않았다.

## 3. J2K 표 (카드 항목 2)

PS3.5 표 8.2.4-1 은 MONOCHROME J2K 에서 `BitsAllocated 40` 을 허용하면서 `BitsStored 1–38`, `HighBit 0–37` 로 제한한다. 지금까지 J2K 분기는 `BitsStored ≤ BitsAllocated` 만 보고 `BitsAllocated` 가 8·16 이 아니면 바로 `UNSUPPORTED` 로 분류해 `40/39/38`, `40/40/39` 를 "유효하지만 미지원"으로 잘못 분류했다. `HighBit = BitsStored − 1` 이 이미 검사되므로 두 상한은 하나(`BitsStored > 38`)다. 이를 미지원 검사보다 먼저 `DICOM_INVALID` 로 한다.

시험 (182e 의 분류 시험에 행 추가): `40/40/39` → INVALID, `40/39/38` → INVALID, `40/38/37` → UNSUPPORTED (표가 허용, 이 reader 가 안 돌려줌). 비압축은 셋 다 `UNSUPPORTED`, JPEG LL 은 셋 다 `INVALID`(BitsAllocated 40 이 표 8.2.1-2 의 8·16 밖).

**다른 전송 구문 표에 같은 "할당은 허용·저장 상한은 별도" 칸이 있는가** — 이 reader 가 여는 전송 구문별로 표를 다시 읽었다:

| 전송 구문 | 표 | BitsAllocated | BitsStored | HighBit | "할당 허용·저장 상한 별도" 칸 |
|-----------|----|---------------|------------|---------|-------------------------------|
| JPEG Lossless (.4.57, .4.70) | 8.2.1-2 | 8 또는 16 | 1–16 | 0–15 | **없음.** BitsAllocated 가 8 이나 16 이면 `BitsStored ≤ BitsAllocated`(8.1.1)가 이미 상한 16 을 보장한다 |
| JPEG 2000 (.4.90, .4.91), MONOCHROME | 8.2.4-1 | 1, 8, 16, 24, 32, 40 | 1–38 | 0–37 | **있음** — 이번에 고침 (40 에서 39·40 이 표 밖) |
| JPEG 2000, PALETTE COLOR 행 | 8.2.4-1 | 8 또는 16 | 1–16 | 0–15 | 위와 같은 이유로 없음 (이 reader 는 PALETTE COLOR 를 182f 에서 거절) |
| 비압축 (Explicit/Implicit LE) | 전송 구문 표 없음 | 8.1.1 의 규칙만 | | | 해당 없음 |

이 reader 가 열지 않는 전송 구문(JPEG 손실, JPEG-LS, RLE, HTJ2K 등)은 `xpe_dicom_open` 이 거절하므로 표를 읽지 않았다.

## 4. 정상 영상 비트 동일과 비용 (카드 "차이 크기를 보고하고 멈출 것")

`regression_hash_and_cost_alternating.txt`, `regression_summary.txt`. 수정 전(HEAD 빌드) DLL 과 수정 후 DLL 을 **번갈아 4회**, 회마다 중앙값.

| 함수 | 수정 전 (ms) | 수정 후 (ms) | 차 | 출력 해시 |
|------|--------------|--------------|----|-----------|
| bilateral 3072² | 99.45 | 103.10 | +3.65 (+3.7%) | **동일** (4회 전부) |
| edge_enhance 3072² | 16.66 | 18.47 | +1.81 (+10.9%) | **동일** |
| multiscale 3072² | 244.30 | 244.71 | +0.41 (+0.2%, 오차) | **동일** |
| NLM 96² (창 11, 패치 5) | 35.17 | 34.69 | −0.48 (오차) | **동일** |

정상 영상의 출력은 바뀌지 않았으므로 "멈출 것" 조건은 이 방식에서 해당하지 않는다. edge_enhance 의 +1.81 ms 는 이 함수가 원래 화소를 한 번도 먼저 훑지 않는 구조라서(제자리 쓰기 전에 훑어야 함) 생기는 비용이며, 18.5 ms 는 코드 주석이 적은 모듈 예산 20 ms 안이다. 181f 에서 리더가 받아들인 "+2 ms 안팎" 범위다.

## 5. 증거

- **시험**: enhance_basic `NonFinitePixels.*` 에 6건 추가(bilateral 2, NLM 2, edge 2) + NLM `h_param` 1건 — 13건에서 20건으로. enhance_advanced 에 multiscale 2건 — 현재 5건. dicom 은 182e 분류 시험에 3행.
- **빨강 → 초록**: 시험을 먼저 만들어 빨강을 확인한 뒤 구현했다 (`red_before_fix.txt`: enhance_basic 6건, enhance_advanced 2건, dicom 1건 빨강).
- **반증** (`arms_check_removed.txt`): 새 검사 10곳을 하나씩 약화시켜 해당 시험을 돌렸다. enhance_basic 은 출하 `/fp:fast` 와 `/fp:precise` 둘 다, 나머지 모듈은 `/fp:fast` 를 쓰지 않아서 한 번.

  | 약화한 검사 | 빨강이 된 시험 |
  |-------------|----------------|
  | B1 bilateral 비유한 화소 | `BilateralRefusesANonFinitePixel…` (두 모드) |
  | B2 bilateral 합이 float 을 넘을 수 있음 | `BilateralRefusesAnImageWhoseWeightedSum…` (두 모드) |
  | N1·N2 NLM 같은 두 검사 | NLM 시험 각 1건 (두 모드) |
  | N3 NLM `h_param` 유한성 | `NlmRefusesANonFiniteHParam` (두 모드) |
  | E1·E2 edge_enhance 같은 두 검사 | edge 시험 각 1건 (두 모드) |
  | M1 multiscale 결과 검사 | multiscale 시험 2건 |
  | M2 multiscale 거부 보고 | multiscale 시험 2건 |
  | J1 J2K `BitsStored > 38` | 분류 시험 |

  소스는 매번 원복했고 바이트 일치를 단언했다. 대조군 전부 초록 (enhance_basic 20, enhance_advanced 5, dicom 89).
- **전체**: `ctest --test-dir build/ci-post` `100% tests passed, 0 tests failed out of 1085` (25건 ONNX 의존 `Skipped`, ci-post 는 스텁 AI 빌드이며 AI 동작 검증이 아니다). `ctest --test-dir build/ci-dicom` `100% tests passed, 0 tests failed out of 236` (건너뜀 1건 기존).

## 6. 바뀌는 반환 코드 (레인 간 계약)

| 함수 | 이제 거부하는 입력 | 이전 |
|------|--------------------|------|
| `xpe_noise_reduce` bilateral | NaN/±∞ 화소 · 가중합이 float 을 넘을 수 있는 영상 (`M × ksize × 1.01 > FLT_MAX`, σ=3 이면 `M > 약 2.6e37`) | rc=0, 비유한 영상 |
| `xpe_noise_reduce` NLM | 〃 (`M × 창² × 1.01 > FLT_MAX`) · NaN/+∞ `h_param` | 〃, +∞ `h_param` 은 가중치 전부 `exp(-dist2×0)` |
| `xpe_edge_enhance` | NaN/±∞ 화소 · `M > FLT_MAX/12.12` (약 2.8e37). `amount=0` 의 무동작은 입력을 보지 않고 OK | rc=0, 비유한 영상 |
| `xpe_multiscale_process` | 결과가 비유한이 되는 영상 (NaN 입력 포함) | rc=0, 비유한 영상 |
| `xpe_dicom_read_image` | J2K 에서 BitsStored 39–40 (BitsAllocated 40) | `UNSUPPORTED_FORMAT` → `DICOM_INVALID` |

의료 영상의 실제 값(16비트에서 온 float)은 이 상한보다 수십 자릿수 아래라 현실의 입력에는 영향이 없다고 **판단**하지만, 실제 파이프라인에서 확인하지는 않았다. 클라이언트 확인은 181f·182e 와 같다 (`clients/` 가 이 함수들의 반환 코드로 분기하거나 알림 문구를 고정한 곳이 없음, GUI 미실행).

## 7. Gaps

- double 누산 방식의 정상 영상 차이 크기와 속도는 **측정하지 않았다** (구성상 비트가 바뀐다는 것만 안다).
- 훑기는 작은 영상·5개 패턴·열거한 파라미터에 한정 (§1).
- bilateral 의 `ksize` 상한은 "모든 가중치 ≤ 1" 이라는 코드 읽기에서 나온 보수적 상한이다. 실제 가중치 합(σ=3 에서 약 7.5)보다 크다. 그래서 거부 경계가 이론적으로 필요한 것보다 낮다(약 1.7배).
- NLM 은 `search_window` 가 매우 큰 경우(int 한계 근처)를 시험하지 않았다. 상한은 double 로 계산해 정수 넘침은 없다.
- 시험은 합성 영상이다. 실제 장비 영상은 쓰지 않았다.
- `xpe_gsdf_calibrate`, `xpe_apply_presentation_lut` 의 최악 입력 훑기는 하지 않았다 (§1).

## 8. Residual-risk

- ±FLT_MAX 급 입력은 이제 거부된다. 그런 입력을 정상으로 처리해야 하는 호출자가 있다면(없다고 판단) double 누산 방식이 필요하다.
- 거부 경계가 보수적이라 상한 근처의 영상은 계산 가능한데도 거부될 수 있다.
- 측정은 이 기계의 MSVC 14.44 한 CPU 의 값이다.
