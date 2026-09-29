# QA-A-104 — 비선형 보정 표시 결함(#184), 파이프라인 0 출력(#185), FUNC-006 조사, 교정 적재 시간(#179)

커밋 `081d1ee`(수정), `9754d9d`(시험). dev/preprocess, 미푸시, 기준 `ce6924e`.

## 1. 주장

### 1.1 #184 — 거짓 `XPE_FLAG_NONLINEARITY_CORRECTED`
1. "실제로 보정했는지"를 알리는 방법으로 **내부 함수** `xpe_nonlinearity_apply(img, cfg, bool* applied)` 를 추가했다(`xpe_preprocess_internal.h`).
   - 공개 `xpe_nonlinearity_correct` 는 이 함수를 부르는 얇은 래퍼다. 시그니처와 반환값은 그대로이며, 공개 ABI 변경은 없다.
   - `applied` 는 화소를 바꿨을 때만 true 가 된다. 현재는 어떤 설정에서도 false 다(FUNC-006 미구현).
2. 파이프라인은 `applied` 가 true 일 때만 플래그를 켠다. 그래서 지금은 켜지지 않는다.
3. 이 플래그를 읽는 곳(검색: 저장소 전체, `NONLINEARITY_CORRECTED|0x40u|0x0040`)

| 위치 | 종류 | 영향 |
|---|---|---|
| `modules/common/include/xpe/common/xpe_types.h:162` | 정의 | 없음 |
| `modules/preprocess/src/pipeline.cpp` | 설정 | 수정함 |
| `modules/preprocess/tests/test_pipeline_stages.cpp:122` | 시험(켜짐을 단언) | 꺼짐 단언으로 바꿈 |
| `docs/project/api-spec.md:136`, `.moai/project/api-spec.md:136` | 정의 문서 | 없음 |
| `docs/calibration/README.md:216, 446` | 설명(1.5단계 후 켜짐) | 현재 동작과 다름 — lead 소유, 수정하지 않음 |
| `.moai/project/codemaps/data-flow.md:240` | 예시 코드(플래그가 꺼져 있으면 보정 실행) | lead 소유, 수정하지 않음 |
| `clients/`, `gui/` | 플래그를 읽는 곳 없음(GUI 는 `NonlinearityCorrectionMode` 설정만 있음) | 없음 |

### 1.2 #185 — 파이프라인 0 출력
4. 원인 세 곳. 모두 "단계 함수가 원본 버퍼를 제자리에서 고친 뒤, 비어 있는 다음 버퍼를 넘긴다"는 같은 형태다.
   - 비선형: stage2 결과를 stage3 로 복사하는 조건이 `if (stage2Data.empty())` 였다. 오프셋이 켜진 기본 설정에서는 0으로 채운 버퍼가 게인 단계에 갔다.
   - 비닝(모드 >1): stage4 를 제자리에서 고치고 빈 stage5 를 다음 단계에 넘겼다.
   - 고스트(핸들 있음): stage6 을 제자리에서 고치고 빈 stage7 을 원본에 되돌려 썼다.
5. 수정: 각 단계가 자기 버퍼에 앞 단계 결과를 복사한 뒤 그 버퍼를 고친다.
6. 기존 시험이 못 잡은 이유
   - 출력 값을 보는 파이프라인 시험은 둘뿐이었다: `PipelineExTest.PipelineExWithState`(`test_pipeline_ex.cpp:209-223`), `CalibFixtureGenTest.GeneratedSetDrivesThePipeline`(`test_calib_fixture_gen.cpp:180-187`).
   - 둘 다 `bypassNonlinearity` 와 `bypassGhost` 를 true 로 두어, 고장 난 두 단계를 거치지 않았다.
   - `test_pipeline_stages.cpp` 는 플래그만 본다.
   - 비닝 값을 보는 시험은 없었다.
   - 참고: `test_pipeline_stage_values.cpp` 는 이 카드에서 새로 만든 파일이다.

### 1.3 FUNC-006 조사(판독만) — §4
### 1.4 교정 적재 — §2.5

## 2. 증거

### 2.1 #184·#185 수정 전 재현
수정 전 코드 `ce6924e`, `BUILD_EXIT=0`(`a104-run-before.log`).
- 플래그 시험: `Expected: false` → FAILED.
- 오프셋+비선형+게인: 기대 450, 실제 0 → FAILED. 비선형만 건너뛰면 450 → 통과.
- 고스트 핸들: 기대 450, 실제 0 → FAILED.
- 원래 `pipeline.cpp` 로 되돌린 빌드(`a104-run-origpipe.log`): 비닝·고스트·플래그·오프셋 값 시험 4건 FAILED.

### 2.2 수정 후
- `BUILD_EXIT=0`, warning C 0.
- 새 시험 14건 통과: 조합 8 + 단계별 5 + 고스트 두 프레임 1(`a104-run-combo2.log`).
- 전체 `ctest --preset ci-preprocess` 678건 통과, 종료 0(`a104-ctest2.log`).

### 2.3 독립 기대값
- v(i) = (raw(i) − 100) / 2, raw(i) = 1000 + 10·i.
- 결함 화소 값은 네 이웃 v 의 평균이다.
- 비닝 모드 2 는 v/4 다.
- 고스트 두 번째 프레임은 별도 핸들로 `xpe_ghost_correct` 를 직접 부른 결과와 비교한다. 대조로, 그 결과가 보정 전 값과 다름을 단언한다.

### 2.4 반증
각각 한 곳만 되돌리고 다시 빌드했다. 모두 `BUILD_EXIT=0`, warning 0.

| 되돌린 것 | 빨강이 된 시험 |
|---|---|
| stage3 복사 조건 원복 | 오프셋값 1 + 조합 NonlinOn 4 = 5건 |
| 고스트 원복(복사 없음, stage6 보정) | 고스트 단일 1 + 조합 Ghost 4 = 5건 |
| 고스트 보정 대상만 stage6 (복사 유지) | 두 프레임 시험 1건 |
| 비닝 대상 stage4 원복 | 비닝 1건 |
| 플래그 무조건 켜기 | 플래그 시험 2건 |

"고스트 보정 대상만 stage6" 1차 반증은 처음 시험 세트에서 빨강이 되지 않았다. 첫 프레임에서는 고스트 보정이 값을 바꾸지 않기 때문이다. 그래서 두 프레임 시험을 추가했고, 그 뒤 빨강이 됐다.

### 2.5 파이프라인 시간 재측정 (#185 수정 후)
QA-A-102/103 의 파이프라인 값은 0 출력 상태에서 잰 것이라 무효다.

| 항목 (3072², 15회 중앙값) | ms |
|---|---|
| `pipeline_ex` 고스트 없음 / T1 / T2 / T3 | 123.9 / 152.9 / 202.5 / 258.4 |
| `pipeline` (프레임마다 3파일 적재) 고스트 없음 / T3 | 586.9 / 800.1 |
| 교정 3파일 적재 | 503.8 |

동시 부하: 측정 전 CPU 10%, GUI 레인 `dotnet` 1개(`pipeline-after.log`).

### 2.6 교정 적재 분해
`lbench.cpp`: `read_xcal_file` 과 로더 단계를 같은 호출·순서로 옮겨 각각 재고, 실제 API 도 함께 쟀다. 15회 중앙값(`load-breakdown.log`).

| 파일 (크기) | API | 열기+헤더+JSON | 페이로드 읽기 | SHA-256 | 할당+복사 |
|---|---|---|---|---|---|
| offset (37.7 MB) | 225.9 | 0.16 | 34.9 | **188.4** | 11.8 |
| gain (37.7 MB) | 237.6 | 0.18 | 40.5 | **205.2** | 12.3 |
| defect (9.4 MB) | 70.4 | 0.14 | 10.4 | **54.0** | 3.3 |

- 세 파일 합계 약 534 ms 중 SHA-256 이 약 448 ms(84%) 다. PicoSHA2 로 약 190 MB/s 다.
- 요구 원문(SRS-CALIB-001:286 PERF-003)
  - "Calibration file load time (CalibManager initialization) shall not exceed 200 ms for all three files (offset, gain, BPM) on SSD. … CRC-32 validation shall be incremental (calculated during read, not post-hoc)."
  - 근거 열: "Clinical workflows load calibration once at startup, not per-frame."
- 판독
  - 요구는 한 번 적재해 두는 사용을 전제한다. `xpe_preprocess_pipeline(calibPath)` 는 프레임마다 3파일을 다시 읽는다(`pipeline.cpp:308-330`). 요구의 사용 방식과 맞는 것은 한 번 적재한 뒤 `pipeline_ex` 를 쓰는 경로다.
  - 요구는 CRC-32 를 적었고, 구현은 SHA-256 이다(REQ-P1A-014~016, `xpe_sha256.hpp:8`). 서로 다른 요구가 부딪힌다.
- 코드는 바꾸지 않았다. 출력이 같은 개선(다른 SHA-256 구현)은 가능하지만, 의존성과 설계 선택이 따르므로 선택지로만 적는다.

| 선택지 | 기대 효과 | 영향 |
|---|---|---|
| A. SHA-256 을 OS 구현(Windows CNG `BCryptHash`)이나 SHA-NI 가속 구현으로 교체 | 해시 부분은 CPU 에 따라 수 배 빨라질 수 있다(미측정) | 다이제스트가 같아 파일 호환 유지. 플랫폼별 코드와 의존성이 는다 |
| B. 요구대로 증분 CRC-32 로 바꾸거나, CRC-32 와 SHA-256 을 병행 | CRC-32 는 SHA 보다 훨씬 빠르다(미측정) | 파일 형식 변경. REQ-P1A-014 SHA 요구와 충돌 → 요구 정리 필요 |
| C. 읽기와 해시를 한 번의 스트리밍 패스로 합침(요구의 "calculated during read") | 페이로드 읽기 후 복사 한 번 감소(해시 비용은 그대로) | 코드 구조 변경 |
| D. 프레임마다 적재하는 `pipeline(calibPath)` 사용을 문서로 막고, `calib_state_load` + `pipeline_ex` 를 권장 | 프레임당 비용 0 | API 사용 방식 변경(호출자는 저장소 안에 시험뿐) |
| E. `make_unique<float[]>` 0 초기화를 없앰(`new float[n]`) | 할당+복사 ~27 ms 중 일부 | 출력 동일 |

## 3. 기준 귀속
- 기기와 빌드는 QA-A-102 와 같다(i7-12700, RelWithDebInfo).
- 수정 전 재현은 `ce6924e` 트리의 `pipeline.cpp` 원본(보관본 `a104-pipeline.cpp.orig`)으로 빌드했다.

## 4. FUNC-006 조사 (판독, 결정은 lead)

### 4.1 원문과 범위 제외 근거
- SRS-CALIB-001:42 FUNC-006: "System shall apply nonlinearity correction using lookup table (LUT) or monotonic polynomial fitting before gain correction. `I_lin(x,y) = f_nonlin(I_raw(x,y))` where `f_nonlin` is detector-specific and stored in calibration profile. LUT shall have minimum 256 entries; polynomial degree ≤ 5."
- FUNC-006-EXT(:44-93)
  - 6a LUT(4096 또는 65536 항목, uint16): N ≥ 10 선량 평탄 영상의 평균 신호로 이상적 직선 `S_ideal = G_nominal × D` 에 맞춘다. 단조 3차 스플라인(Fritsch–Carlson)으로 보간한다. 단조성을 강제한다.
  - 6b 4차 **전역** 다항식(Horner).
  - 6c 선택 로직 `panel.nonlinearity_mode`.
- SPEC-XPE-P1A `spec.md:55` "PRE-08: Nonlinearity Correction -- 별도 SPEC", `:560` "`xpe_nonlinearity_correct()` | Separate SWU | SPEC-XPE-P1D".
- `.moai/specs/` 에 SPEC-XPE-P1D 는 없다(`ls` 결과 0건). 제외 사유는 "별도 SWU" 한 줄뿐이다.

### 4.2 일반적인 방법(출처)
- 화소별 다항식
  - Altunbas 외(2014, Med. Phys. 41(9), doi:10.1118/1.4893278)는 CBCT 평판 검출기에서 필터 두께를 바꿔 선량과 스펙트럼을 변화시킨 평탄 영상으로 "ideal" 화소값을 **다항식 맞춤**으로 추정했다.
  - 그 잔차로 선량 의존 화소 이득 변동을 보정(pixel gain correction)한다. 기존 평탄화 보정이 잡지 못하는 선량 의존 이득 변동이 대상이다.
- 신호 의존 보정 일반형
  - van Driel 외(2015, J. Synchrotron Rad., PMC4416674)는 여러 강도의 일정 신호로 만든 교정 데이터셋에서 신호 의존 비선형 응답을 모델 독립적으로 보정했다.
  - 교정 함수는 교차 교정점 주변 **다항식(테일러) 근사**로 매개화했다(§5, §7).
  - 이 논문은 X선 자유전자 레이저용 화소 배열 검출기 사례라 평판 검출기와 조건이 다르다.
- 평판 검출기 교정 절차 일반: 오프셋·게인 외에 비선형을 별도 보정 대상으로 다룬다(NDT.net WCNDT 2000 논문 "High Resolution Digital Flat-Panel X-Ray Detector": 12비트 ADC, 보정 후 불균일도 < 1%). 검색 요약에 나온 "포화 선량 절반 부근의 세 번째 교정 영상으로 화소별 이중 선형 보정"은 본문에서 해당 문단을 찾지 못해 **미확인**이다.
- 정리: 방법은 (a) 검출기 전체에 하나의 LUT/다항식(SRS 6a/6b 방식), (b) 화소별 다항식(이득의 선량 의존성까지 흡수), 두 갈래다. 계수는 모두 **여러 선량의 평탄 영상**으로 얻는다.

### 4.3 이 코드베이스와의 겹침
- `xpe_calib_generate_gain_polynomial`(FUNC-027) 은 N ≥ 3 선량의 **게인 맵**(정규화 평탄 영상)을 받아 화소별 G(x,y,D) 다항식을 맞춘다. FUNC-031 모드로 레벨 수와 차수를 제한한다.
  - 즉 "여러 선량 평탄 영상" 교정 데이터와 화소별 다항식 맞춤 코드는 이미 있다.
- 적재: `xpe_calib_load_gain` 은 `XCAL_TYPE_GAIN_POLY` 를 받아 `g_calib.gain_poly_coeffs` 에 넣고 스칼라 게인 맵을 지운다(`xpe_calib_load_gain.cpp:76-90`).
  - 그런데 게인 보정은 다항식을 적용하지 않는다. 주석대로 `xpe_gain_correct()` 가 CALIB_NOT_LOADED 를 낸다. **다항식 게인은 만들고 읽지만 쓰는 곳이 없다.**
- 차이
  - FUNC-006 은 신호 영역(raw ADU → 선형 ADU)의 **전역** 함수이고, uint16 에 게인 **이전**에 적용한다.
  - FUNC-027 은 선량 영역의 **화소별** 이득이고, 적용하려면 프레임의 선량(D)이 필요하다. `XpeImageMetadata` 에는 kVp/mAs 만 있고 선량이 없다.
  - FUNC-006 LUT 생성은 각 선량의 **평균 신호**와 기준 선량이 필요하다. 현재 게인 맵 파일은 평균 1로 정규화되어 있어(`xpe_calib_generate_gain.cpp:278-286`) 원래 신호 크기를 잃는다.

### 4.4 구현 선택지(결정은 lead)

| 선택지 | 내용 | 영향 |
|---|---|---|
| 1. SRS 6a 대로 전역 LUT | 새 XCal 형식(NONLIN_LUT, uint16 × 4096/65536), 생성 함수(선량별 평균 신호·기준 선량 입력, Fritsch–Carlson 보간, 단조 검사), 적재, 파이프라인 3단계 적용, `applied=true` | 요구와 1:1. 새 파일 형식·공개 API 추가. 교정 절차(N ≥ 10 선량)를 GUI/문서에 추가해야 함. 3072² 에서 표 조회라 시간 부담 작음 |
| 2. SRS 6b 전역 다항식 | 계수(≤4차)를 교정 프로파일 JSON 에 두고 Horner 로 적용 | 파일 형식 변경 없이 설정 경로로 가능. 계수 생성 도구가 따로 필요. 비단조 시 LUT 로 폴백하라는 요구 → 1과 함께 가야 완전 |
| 3. FUNC-027 화소별 다항식을 실제로 적용 | 게인 단계에서 G(x,y,D) 적용 | 요구 FUNC-006 의 구현은 아님(선량 영역, 화소별). 선량 입력이 메타데이터에 없어 API 확장 필요. 이미 생성·적재되는 데이터가 쓰이게 됨 |
| 4. 요구를 현재 상태에 맞춤 | FUNC-006 을 "미지원/후속"으로 표기 | 사용자 원칙(스펙이 맞으면 구현 방법을 찾는다)과 반대 |

1과 3은 교정 데이터(여러 선량 평탄 영상)를 공유할 수 있다. 다만 1은 정규화 전 평균 신호를 따로 보존해야 한다.

## 5. 미검증 · 잔여 위험
- SHA-256 대체 구현, CRC-32 의 실제 시간은 재지 않았다(선택지의 기대 효과는 추정).
- 고스트 첫 프레임 무변화는 이 시험들의 1e-3 허용치에 기대고 있다. 고스트 매개변수가 바뀌면 조합 시험의 허용치를 다시 봐야 한다.
- 비닝 모드 4, 온도 보정과 판독 검사를 켠 조합은 값 시험에 넣지 않았다(시험 버퍼가 float 크기라 온도 단계의 `img->dataSize` 복사와 맞지 않음). 온도 단계의 `memcpy(..., img->dataSize)` 는 dataSize 가 W·H·2 보다 크면 넘칠 수 있다(`pipeline.cpp:122`). 판독 결과이며 재현하지 않았다.
- `docs/calibration/README.md:216,446`, `codemaps/data-flow.md:240` 은 플래그가 늘 켜진다고 적고 있다(lead 소유).
- 웹 출처 중 NDT.net 본문의 "세 번째 교정 영상" 문구는 확인하지 못했다.

## 출처
- [Altunbas et al. 2014, OSTI 22409550](https://www.osti.gov/biblio/22409550)
- [van Driel et al. 2015, PMC4416674](https://pmc.ncbi.nlm.nih.gov/articles/PMC4416674/)
- [NDT.net WCNDT 2000, High Resolution Digital Flat-Panel X-Ray Detector](https://www.ndt.net/article/wcndt00/papers/idn615/idn615.htm)
