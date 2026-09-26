# QA-B-59 게이트 보고서 — 의존성 전수의 나머지 절반 (dicom · ai · gsvg)

**카드**: QA-B-59 (#156 #142)
**레인**: Lane B (`xpe-post`, `dev/postprocess`)
**증거 경로**: `.moai/reports/lane-post/QA-B-59/`
**커밋 1건**: `3d27cf1`
**선행**: `git merge origin/main` 완료 (B-58 병합 `b1936ba`)

---

## 0. 결론 먼저

**네 번째 상쇄는 없었다.** 측정한 13개 축이 전부 출력에 도달한다.
다만 두 가지는 "도달했다" 로 셀 수 없어 별도로 적었다 — **ai 의 추론 진입점(측정 불가)** 과
**ai 의 fallback 모드(관측 가능한 차이 없음, 단 이 빌드에서)**.

---

## 1. 후보 전수

| 모듈 | 축 | 파일·줄 | 후보인 이유 |
|---|---|---|---|
| gsvg | `vignette_correction` | `gsvg.cpp:190` | **기본값 false** — B-52 가 이 기본값 때문에 "헤더가 틀렸다" 로 적을 뻔했다 |
| gsvg | `grid_suppression` | `gsvg.cpp:191` | 같음. 게다가 억제가 **행편차 1.0 미만이면 건너뛴다** → 픽스처가 바닥 아래면 무반응처럼 보인다 |
| gsvg | `gainMap` 내용 | `gsvg.cpp:218` | 플래그와 맵이 **둘 다** 있어야 동작 — 두 원인이 한 결과에 겹친다 |
| gsvg | 미지 config 키 | `gsvg.cpp:56` | 파서가 불리언 leaf 만 읽는다 — 문서가 말하는 다른 키들이 **조용히 무시되는지** |
| ai | `xpe_stitch_estimate_size` 인자 | `ai.cpp:411~` | 인자 산술이라 **stub 에서도 완주**한다 |
| ai | `xpe_ai_set_fallback_mode` | `ai.cpp:603` | 모듈 상태를 stub 자신이 참조한다(`:375-378`) |
| ai | 추론 3종 | `ai.cpp:359, 496, 533` | **검증 뒤 모델 전에 빠진다** — "무반응" 과 구분 필요 |
| dicom | 영상·치수 | `DicomWriter.cpp` | write 계열에 튜닝 파라미터가 없다(B-54) — 대신 이것이 입력이다 |
| dicom | `meta` 5필드 | `DicomWriter.cpp:172~` | 읽히고 운반되지만 **기록되지 않는 필드**가 같은 실패 형태다 |

---

## 2. 픽스처 바닥 확인 (카드 3항)

**"출력이 안 움직였다" 는 픽스처가 바닥을 넘었을 때만 발견이다.** B-58 §3 의 두 가짜 발견이
그 자리에서 나왔으므로, 이번에는 바닥을 **단언으로 먼저 고정**했다.

| 대상 | 바닥 | 확인 |
|---|---|---|
| gsvg 그리드 억제 | 행평균 편차 **> 1.0** (`suppress_grid_row_mean`, `kDeviationThreshold`) | 픽스처 최대 편차 **50** — `FixtureRowsDeviateAboveTheSuppressionThreshold` 가 먼저 단언한다 |
| gsvg vignette | **플래그 + 게인맵 둘 다** 필요 | 두 실행 모두 게인맵을 공급하고 **플래그만** 바꿨다 |
| dicom 왕복 | 동일 입력이 동일하게 돌아와야 한다 | `RoundTripReproducesTheInput` 가 먼저 단언한다 |

균일 영상으로 잤다면 gsvg 억제는 편차 0 → 분기 스킵 → **무반응**이 나왔을 것이고,
그것은 플래그가 안 닿는 것이 아니라 **함수가 일할 기회를 못 얻은 것**이다.

---

## 3. 측정 결과

### 3.1 gsvg (`_gsvg.log`, BUILD=0)

| 축 | 결과 | 판정 |
|---|---|---|
| `grid_suppression` off↔on | **4096 / 4096** 화소 변화 | 도달 |
| `vignette_correction` off↔on (게인맵 양쪽 공급) | **4092 / 4096** | 도달 |
| 게인맵 내용 상승↔하강 (플래그 양쪽 on) | **4092 / 4096** | 도달 |
| 미지 키만 담은 config vs config 없음 | **0 / 4096** | 기본값 불변 — 의도대로 |

마지막 줄이 뒤집힌 형태의 확인이다: **문서가 말하는 `grid_frequency_lp_per_mm` ·
`virtual_grid_enabled` 같은 키를 줘도 아무것도 켜지지 않는다.** 파서가 그 이름을 모르기
때문이고, 그것이 올바른 동작이다(§6 에 문서 쪽 기록).

### 3.2 ai (`_ai.log`, BUILD=0)

| 축 | 결과 | 판정 |
|---|---|---|
| `xpe_stitch_estimate_size` 부품 너비 256→512 | 435 → **870** | 도달 |
| 〃 높이 128→256 | 128 → **256** | 도달 |
| 〃 부품 수 2→3 | 435 → **614** | 도달 (문서화된 +70% 휴리스틱과 일치) |
| `xpe_ai_set_fallback_mode` 1↔0 | 양쪽 `rc=-3`, `'UNKNOWN'`, `conf=0` | **관측 가능한 차이 없음 — 단 §3.3** |
| `xpe_bodypart_recognize` · `xpe_bone_suppress` · `xpe_dl_denoise` | 전부 `rc=-3` | **측정 불가 — §3.3** |

### 3.3 "도달 못 함" 과 "무반응" 은 다르다

stub 빌드에서 추론 진입점은 **인자 검증 뒤, 모델 전에** `XPE_ERR_PROCESSING_FAILED` 로
빠진다(REQ-AI-002 fallback 라우팅). 거기서 파라미터를 쓸어 본들 재는 것은 **검증 순서**이지
모델이 아니다.

- **결함이 아니다** — 설계된 stub 동작이다.
- **정상의 증거도 아니다** — 모델에 닿지 않는 입력은 영향이 있다고도 없다고도 보일 수 없다.

그래서 표에서 빼지 않고 **"이 빌드에서 측정 불가"** 로 이름을 적었고,
`InferenceEntryPointsStopBeforeTheModel` 이 그 상태를 단언으로 고정한다 — 언젠가 ONNX 경로가
살아나면 **이 단언이 먼저 깨지면서** 해당 파라미터들이 측정 대상이 됐음을 알린다.

fallback 모드도 같은 자리에 있다: 설정은 양방향으로 수락되지만 이 빌드에서 관측 가능한
차이를 만들지 않는다. **그 효과는 ONNX 빌드의 성질이고**(#130 미검증), stub 에서의 무차이를
"도달하지 않는다" 로 적으면 §3.3 의 구분을 스스로 무너뜨린다.

### 3.4 dicom (`_dicom.log`, BUILD=0)

write 계열에는 **튜닝 파라미터가 없다**(B-54 가 잰 3인자 형태 — 문서가 말하던
`compressionRatio` · `configJsonOrNull` 은 헤더에 없었다). 대신 영상과 메타데이터가 입력이고,
같은 실패 형태가 적용된다: **읽히고 운반되지만 기록되지 않는 필드.**
채널은 파일이다 — 써서 다시 읽어 차이가 왕복을 견디는지 본다.

| 축 | 결과 | 판정 |
|---|---|---|
| 화소값 1000↔4000 | 첫 샘플 1000 → **4000** | 도달 |
| 치수 32×96 | 읽기 **32×96** | 도달 |
| `bodyPart` CHEST↔ABDOMEN | 'CHEST' → **'ABDOMEN'** | 도달 |
| `kVp` 80↔120 | 80 → **120** | 도달 |
| `mAs` 2.5↔10 | 2.5 → **10** | 도달 |
| `SID_mm` 1800↔1000 | 1800 → **1000** | 도달 |
| `pixelPitch_mm` 0.148↔0.200 | 0.148 → **0.2** | 도달 |

---

## 4. 임계값 근거 (카드 2항)

B-58 은 float 출력에 절대값 **1e-4** 를 썼고, 그 근거는 "1 ulp 차이는 파라미터가 답이 아니라
반올림에 도달했다는 뜻" 이었다. **이 카드의 축은 출력 스케일이 달라 그대로 쓰지 않았다:**

| 모듈 | 출력 종류 | 이 카드의 판정 기준 | 이유 |
|---|---|---|---|
| gsvg | `uint16_t` 화소 | **다른 화소 개수 > 0** | 정수라 ulp 문제가 없다. 1 코드값 차이도 실재하는 차이다 |
| ai (estimate) | `uint32_t` 치수 | **`!=`** | 같음 |
| dicom | 문자열·`float` 태그·`uint16` 화소 | **`!=` / `STRNE`** | 왕복을 견딘 값의 동일성이므로 근사 비교가 의미 없다 |

**B-58 이 잔여 위험으로 남긴 "같은 절대값을 스케일이 다른 함수에 쓴다" 는 여기서 해소된다** —
이 카드의 축은 전부 이산값이라 임계값 자체가 필요 없다.

---

## 5. 반증 (`_falsify.log`)

`DicomWriter.cpp:174` 의 kVp 기록을 `* 0.0 + 80.0` 으로 **약화**(삭제 아님 — 필드는 계속
읽히므로 `/WX` 가 깨지지 않는다):

```
===BUILD=0===
bodyPart 'CHEST' vs 'ABDOMEN' | kVp 80 vs 80 | mAs 2.5 vs 10 | SID 1800 vs 1000 | pitch 0.148 vs 0.2
kVp does not reach the file
[  FAILED  ] DicomParameterDependency.MetadataFieldsReachTheFile
[       OK ] RoundTripReproducesTheInput / PixelDataReachesTheFile / ImageDimensionsReachTheFile
```

**kVp 만 붙고 나머지 네 필드는 그대로 갈린다** — 축이 서로 독립이며 하나가 죽어도 다른
축이 그것을 가려 주지 않는다는 것이 같은 로그 한 장에 있다. 반증 뒤 원복했다.

---

## 6. Baseline 귀속 (Baseline-attribution)

| 항목 | 값 | 근거 |
|---|---|---|
| 측정 축 | **13개** (gsvg 4 · ai 3 · dicom 7, 중복 제외) | §3 |
| 미도달(상쇄) | **0건** | §3 |
| 측정 불가 | ai 추론 3종 + fallback 모드 | §3.3 |
| 픽스처 바닥 | 행편차 **50 > 1.0**, 왕복 재현 확인 | §2 |
| 반증 | BUILD=0, 약화한 축만 실패 | `_falsify.log` |
| 카드 정정 | `voi_lut.cpp:46` REQ-DISP-011 → **010** | `git show` |
| 이전 ctest | 498 / 211 / 173 | QA-B-58 `_verify.log` |
| 현재 ctest | **503 / 214 / 177** (신규 13건) | `_verify.log` |
| 빌드 경고 | 0 | `_verify.log` |

---

## 7. 미검증 (Gaps)

- **ai 의 추론 파라미터는 이 빌드에서 측정할 수 없다**(§3.3). ONNX 경로(#130)가 서기 전까지
  "모델에 도달하는가" 는 열린 질문이고, **stub 통과를 그 답으로 읽으면 안 된다.**
- **`xpe_ai_get_model_card` · `xpe_dicom_cstore` · `xpe_dicom_cfind_mwl` 은 재지 않았다.**
  앞의 것은 모델 자원이, 뒤의 둘은 **PACS 서버**가 있어야 한다. 오프라인에서 잴 수 있는 것은
  거절 경로뿐이고 그것은 이 카드의 질문이 아니다.
- **`xpe_dicom_write_j2k` 의 왕복은 재지 않았다** — 비압축 write 로만 쟀다. J2K 경로는
  B-50·B-51 이 치수 일치를 다뤘고, 메타데이터 왕복은 같은 writer 를 지난다는 **판독**이다.
- **gsvg 의 config 는 불리언 두 개가 전부다.** 문서가 말하는 `gridFrequency_lp_per_mm` ·
  `virtual_grid_*` 는 **파서가 읽지 않는다**(§3.1 이 무시됨을 실측). 이는 B-54 계열의
  문서 쪽 사안이므로 **목록만 남긴다 — leader 처리**: `api-spec.md:180`,
  `api-spec §12.7 gsvg_virtual_grid`, `docs/post-processing/gsvg/README.md:601-602, 708`.
- **"도달한다" 가 "올바르다" 는 뜻이 아니다**(B-58 과 같은 한계). 이 사전은 영향 유무만 본다.

---

## 8. 잔여 위험 (Residual-risk)

- **네 번째 상쇄가 없다는 것은 "측정한 축에 없다" 는 뜻이다.** 잴 수 없었던 축(§7)이 남아
  있고, 세 번의 전례가 전부 **잴 수 있게 만든 뒤에야** 드러났다.
- **ai 는 사실상 미검증 상태로 남는다.** 진입점 3종이 모델 전에 빠지므로, 이 모듈에서
  "파라미터가 답에 도달한다" 는 문장은 아직 어느 방향으로도 세울 수 없다.
- **gsvg 의 설정면이 문서보다 훨씬 좁다.** 호출자가 문서를 보고 `virtual_grid_enabled` 를
  넘기면 **오류 없이 무시된다** — §3.1 이 그것을 측정했다. 조용한 무시는 조용한 오작동의
  전 단계다.
- **dicom 왕복은 이 writer/reader 쌍에 대한 것이다.** 다른 구현이 쓴 파일, 다른 구현이 읽는
  우리 파일은 이 측정의 범위 밖이다.
- 커밋은 push 전까지 미푸시 유일본이다.

---

## 부록 — 증거 파일

| 파일 | 내용 |
|---|---|
| `_env.bat` / `_verify.bat` | 환경 + 세 프리셋 재빌드·전체 ctest |
| `_b59g.bat` / `_gsvg.log` | gsvg 5건 (바닥 단언 포함) |
| `_b59ai.bat` / `_ai.log` | ai 3건 (측정 불가 기록 포함) |
| `_b59d.bat` / `_dicom.log` | dicom 4건 (왕복 바닥 단언 포함) |
| `_falsify.log` | kVp 기록 약화 — BUILD=0, 해당 축만 실패 |
| `_verify.log` | 최종 503 / 214 / 177, 경고 0 |
