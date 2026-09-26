# QA-B-87 (#180) 게이트 보고서 — SPEC-XPE-GSVG 요구와 코드·시험의 한 줄 대조

**카드**: QA-B-87 · **레인**: Lane B (`xpe-post`, `dev/postprocess`, HEAD `6e9fa6a`)
**성격**: 판독만 한다. 코드 변경, 커밋, 이슈 코멘트는 없다.
**표**: `_table.md` — `_table.py` 가 만들었다. 스크립트는 다음을 단언한다.
- SPEC 제목 26개와 표의 행이 일치한다.
- 모든 칸이 채워져 있다.
- 판정은 세 값 중 하나다.
- "없음" 행에는 검색 패턴이 있다.
- 대조군이 0건이 아니다.

---

## 1. 주장

### 1.1 요구 개수: 카드는 21개, SPEC 제목은 26개

- `spec.md` 에는 `### REQ-GSVG-` 제목이 **26개**(001~026) 있다.
- `**Status**` 줄은 **21개**(001~021)에만 있다. spec.md 에서 `Status` 가 나오는 줄은 52, 61, … 240 과 표 290 이다.
- 022~026(안전 요구)에는 Status 줄이 없다.
- 카드의 "21개"는 Status 가 달린 요구의 수와 같다. 이 보고서는 26개를 모두 대조했다.

### 1.2 판정 집계 (`_table.py` 출력)

| 판정 | 개수 | 요구 |
|---|---|---|
| 구현됨 | 2 | 022, 026 |
| 다른 방식 | 3 | 005, 021, 024 |
| 없음 | 21 | 001~004, 006~020, 023, 025 |

Status 가 달린 21개(001~021)만 따로 세면 이렇다.
- 다른 방식: 005, 021 — 2개
- 없음: 19개
- 구현됨: 0개

### 1.3 요약

- **격자 억제(001~008)**
  - 코드에는 행 평균 차감 하나만 있다(`gsvg.cpp:216-256`).
  - 주파수 검출, DWT, 서브밴드, 가우시안 필터, MTF, lines/inch 범위, 모아레 경로는 없다.
- **가상 격자(009~018)**
  - 경로가 전혀 없다.
  - API 에 노출 인자가 없다(`gsvg.cpp:289-297`).
  - 설정 키는 `vignette_correction`, `grid_suppression` 둘뿐이다(`:280`).
- **성능(019~021)**
  - 019: DWT 경로가 없다.
  - 020: 최대 메모리를 재는 코드도 시험도 없다.
  - 021: 시험 모양이 요구와 다르다(1000회 수명주기, 512², 증가량만 잼).
- **안전(022~026)**
  - 022(원본 보호)와 026(출력 범위)은 코드와 시험이 있다.
  - 024 는 실패할 때 원본을 dst 에 쓰지 않고 오류 코드만 돌려준다.
  - 023 과 025 는 없다.

## 2. 증거

### 2.1 부재 검색

- **범위**: `modules/gsvg/src`, `modules/gsvg/include` 의 `.cpp`, `.h`, `.hpp`
- **분류**: 맞은 줄을 코드줄과 주석줄로 나눴다. 주석줄은 `//`, `*`, `/*` 로 시작하는 줄이다.
- **대조군**: 같은 범위·같은 도구에서 `vignette` 는 **코드줄 6, 주석줄 16** 이다.

| 요구 | 패턴 | 코드줄 | 주석줄 |
|---|---|---|---|
| 001 | `frequen\|pitch\|lp_per_mm\|dicom` | 0 | 2 |
| 002 | `dwt\|wavelet\|haar` | 0 | 0 |
| 003 | `sub.?band\|energy` | 0 | 0 |
| 004 | `gauss\|band.?stop\|notch` | 0 | 2 |
| 006 | `\bmtf\b\|modulation` | 0 | 0 |
| 007 | `lines?.?per.?inch\|\blpi\b\|frequen` | 0 | 1 |
| 008 | `moire\|alias` | 0 | 4 |
| 009 | `thickness\|kvp\|\bmas\b\|\bsid\b` | 0 | 0 |
| 010 | `\bspr\b\|scatter.?to.?primary` | 0 | 0 |
| 011 | `scatter\|kernel\|\blut\b` | 0 | 1 |
| 012 | `scatter\|primary` | 0 | 1 |
| 013 | `laplacian\|pyramid` | 0 | 0 |
| 014 | `denois\|noise\|bilateral` | 0 | 0 |
| 015 | `\bcnr\b\|contrast.?to.?noise` | 0 | 0 |
| 016 | `ratio` | 0 | 4 |
| 017 | `thickness\|acrylic\|\bcm\b` | 0 | 0 |
| 018 | `virtual\|overcorrect` | 0 | 1 |
| 019 | `tier\|dwt\|wavelet` | 0 | 0 |
| 020 | `peak\|512` | 0 | 0 |
| 023 | `processed\|derived\|imagetype\|derivation` | 0 | 0 |
| 025 | `scatter\|\bspr\b\|physical` | 0 | 1 |

추가 대조와 확인:
- **013**: 같은 패턴을 `modules/` 전체에서 찾으면 코드줄이 **0 이 아니다**(enhance_advanced `mfp_scalar.*`). 패턴이 라플라시안 구현을 찾을 수 있다는 뜻이다. `gsvg.cpp` 의 include 는 `gsvg_api.h` 와 표준 헤더뿐이다(`:24-32`).
- **023**: 저장소 전체에서 찾으면 `modules/dicom/src/DicomWriter.cpp:143` 에 `DCM_ImageType` = `"ORIGINAL\\PRIMARY\\"` 가 있다. 이는 gsvg 밖이다.
- **주석줄만 맞은 경우**:
  - `gsvg_api.h:8-9` "anti-scatter grid … (Moire-like) pattern"
  - `gsvg.cpp:14`, `:208` "FFT-based notch filter … future work"
  - `gsvg.cpp:60` "`gridFrequency_lp_per_mm` and `virtual_grid_enabled` — … the documentation describes"

### 2.2 구현됨·다른 방식의 근거 줄

- **005**: `suppress_grid_row_mean` (`gsvg.cpp:216-256`)
  - 전체 평균과 행 평균을 구하고, `|편차| > 1.0` 이면 그 행에서 편차를 뺀 뒤 clamp 한다.
  - 시험 `GsvgCoverage.GridSuppressionReducesPeriodicRowMeanDeviation` 은 처리 전 행0·행1 평균차 > 30, 처리 후 ≤ 1.0 을 단언한다.
  - "visually imperceptible" 자체를 단언하는 시험은 없다.
- **021**: `GsvgEndurance.ThousandCycles_MemoryGrowthUnderOneMB` (`test_gsvg_abi_smoke.cpp:343-386`)
  - 워밍업 100회 뒤 init/process/shutdown 을 1000회 돈다. 512² 영상이다.
  - 작업 집합 증가 < 1 MB 를 단언하지만, 증가했을 때만 단언한다(`if (after > before)`).
  - Windows 가 아니면 SKIP 한다.
  - 요구는 "배치 모드 연속 100 프레임" 이다. 시험은 핸들을 매번 새로 만든다.
  - `RepeatedLifecycleDoesNotLeakOrCrash` 는 32회를 돌고 아무것도 재지 않는다(`:206-208` 주석).
- **022**: `src` 인자가 `const uint16_t*` 다(`gsvg.cpp:290`). 모든 오류 반환(`:303-325`)은 쓰기보다 앞에 있다.
  - 시험 `Lifecycle3072_VignetteAndGrid_OutputClampedAndSourceIntact` 는 처리 후 src 를 memcmp 로 비교한다(`:155-159`).
  - 실패 경로에서 src 를 확인하는 시험은 없다.
- **024**: 실패하면 오류 코드만 돌려주고 dst 에는 아무것도 쓰지 않는다(`:303-325`).
  - 요구 문언 "return the original image unmodified" 와 다르다.
  - 잘못된 JSON 은 실패로 처리되지 않고 기본값(패스스루)과 `XPE_OK` 로 끝난다(`json_get_bool` `:140-171`, 시험 `GsvgEdgeCases.MalformedConfigFallsBackToPassThrough`).
  - 오류 경로 시험(`ProcessRejectsNullImagePointers`, `ProcessRejectsNonPositiveDimensions`, `ShortSourceBufferIsRejectedWithoutBeingRead`)은 반환 코드만 단언한다. 마지막 시험은 읽지 않았다는 것도 단언한다.
- **026**: 두 곳에서 clamp 한다.
  - vignette: `:189-191`
  - grid: `:250-253`
  - 시험 `GsvgCoverage.VignetteGainRoundsAndClampsToUint16Range` 는 입력 {0,1,2,100,32768,65000} × 게인 {−1,1,1.25,0.5,2,2} 의 결과가 {0,1,3,50,65535,65535} 인지 단언한다.

### 2.3 019·020 의 "Measured" 기록 찾기

- **검색**
  - 범위: `.moai/`, `docs/`, `benchmark/`
  - 패턴: `GSVG-0?19|GSVG-0?20|PERF-00[1-4]|peak memory|512 ?MB`
- **맞은 곳은 요구 문장과 추적 행뿐이다. 측정값이 적힌 곳은 없다.**
  - `docs/post-processing/gsvg/GSVG-SRS-001_Requirements.md:55-58` — PERF-001~004 요구 표
  - `GSVG-RTM-001_Traceability.md:61-64` — PERF-001 → ST-006, PERF-002 → IT-005, ST-006. 결과 칸은 없다.
  - `GSVG-SVP-001_Verification_Plan.md:100` — ST-006 계획("≤ 1.0 second"), 결과 없음
  - `GSVG_IEC62304_ClassB_Document_Package.md:200-203` — 요구 표
  - `benchmark/BP-06-09-post-benchmark-baseline.md` — gsvg 는 BP-06 버전 프로브(`1024 version probes under 5000 us`)뿐이다. 영상 처리 시간 수치는 없다.
  - `.moai/reports/lane-post/QA-B-23/gate.md:144` — "REQ-GSVG-019 의 3072×3072 경로에서는 측정하지 않았다."
- **대조**: 같은 검색이 SRS 의 PERF-001 행을 찾았다. 파일이 있으면 잡힌다는 뜻이다.
- **SPEC 표시와의 비교**
  - `spec.md:222`, `:231` 은 "Status: Measured" 다.
  - `spec.md:294` 는 "Performance | PERF-001~004 all PASS | ✅ Measured" 다.
  - 어느 줄도 측정 기록을 가리키지 않는다. 위 범위에서 기록을 찾지 못했다.
- **CI 쪽**: 3072² 시간 단언이 있는 `GsvgAbiSmoke.Lifecycle3072_PerformanceBudget`(< 8000 ms, `:44`)는 ci·benchmark 어느 워크플로에서도 선택되지 않는다(QA-B-85 `_table.md:27`).

## 3. 기준

- 코드와 시험: 작업 트리 HEAD `6e9fa6a` (`dev/postprocess`), 이번 실행
- SPEC: 이 워크트리의 `.moai/specs/SPEC-XPE-GSVG/spec.md`

## 4. 미검증

- 이번 카드에서는 빌드도 실행도 하지 않았다. 시험이 "단언한다"는 것은 본문을 읽은 결과이고, 통과 여부는 QA-B-86 의 ctest(ci-post 545, 실패 0)에 의존한다.
- 부재 검색은 `modules/gsvg` 의 코드 파일만 봤다. gsvg 가 다른 모듈을 링크하는지는 include 줄로만 확인했고 링크 설정은 보지 않았다.
- 주석 분류는 줄 앞머리만 본다. 코드 뒤에 붙은 주석은 코드줄로 센다. 결과가 모두 코드줄 0 이라 판정은 바뀌지 않는다.
- 측정 기록 검색은 저장소 안의 세 경로로 한정했다. CI 로그, 외부 문서, 이슈 본문은 보지 않았다.
- `docs/post-processing/gsvg/README.md` 등 다른 문서에 알고리즘 설명이 있더라도, 코드와의 대조는 SPEC 문언만을 기준으로 했다.

## 5. 잔여 위험

- SPEC 은 21개 요구를 Implemented / Measured / Verified 로 표시한다. 코드에서 요구 문언대로 동작하는 것은 0개다(Status 달린 범위 기준). 이 SPEC 을 근거로 하는 추적 문서(RTM, SVP)도 같은 전제 위에 있다.
- 024 의 동작 차이(원본을 돌려주지 않음)는 호출자가 dst 를 쓰기 전에 반환 코드를 확인해야 안전하다. 호출자 쪽 확인은 하지 않았다.

## 부록 — 증거

`_table.py`, `_table.md`, `gate.md`
