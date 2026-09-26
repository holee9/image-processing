# QA-B-54 게이트 보고서 — 정정문도 재확인 대상이다: post 문서 export·시그니처 전수

**카드**: QA-B-54 (#142 #120)
**레인**: Lane B (`xpe-post`, `dev/postprocess`)
**증거 경로**: `.moai/reports/lane-post/QA-B-54/`
**커밋 1건**: `fac7b8e` — **제품 코드 변경 0**
**선행**: `git merge origin/main` 완료 (B-53 병합, 문서 정정 `84862ff` 포함)

---

## 0. 실측 기준 (ground truth)

dumpbin export(`_exports.log`)와 헤더 `XPE_API` 선언을 각각 뽑아 대조했다.

| 모듈 | dumpbin | 헤더 선언 | 일치 |
|---|---|---|---|
| enhance_basic | 8 | 8 | O |
| enhance_advanced | 7 | 7 | O |
| display | 6 | 6 | O |
| ai | 10 | 10 | O |
| gsvg | 4 | 4 | O |
| dicom | 10 | 10 | O |

**헤더와 DLL 사이에는 불일치가 없다.** 아래의 모든 불일치는 **문서 쪽**이다.

---

## 1. 전수표 — 문서 위치 · 문서가 말하는 것 · 실제 · 판정

기계 대조(`_audit_apispec.log`)는 `docs/project/api-spec.md` 의 모든 `XPE_API`/`GSVG_API`
선언을 헤더 선언과 인자 타입 단위로 비교한다. 이름 대조가 아니라 **시그니처 대조**다.

### 1.1 시그니처 불일치 (7건)

| 문서 위치 | 문서가 말하는 것 | 실제 (헤더) | 판정 |
|---|---|---|---|
| api-spec:738 | `xpe_log_transform(img, const char* configJsonOrNull)` | `(img, float normFactor)` | **시그니처 불일치** |
| api-spec:752 | `xpe_log_inverse(img, const char* configJsonOrNull)` | `(img, float normFactor)` | **시그니처 불일치** |
| api-spec:766 | `xpe_noise_reduce(img, const char* configJsonOrNull)` | `(img, const XpeNoiseReduceParams*)` | **시그니처 불일치** |
| api-spec:794 | `xpe_contrast_enhance(img, const char* configJsonOrNull)` | `(img, const XpeClaheParams*)` | **시그니처 불일치** |
| api-spec:808 | `xpe_edge_enhance(img, const char* configJsonOrNull)` | `(img, const XpeUsmParams*)` | **시그니처 불일치** |
| api-spec:1242 | `xpe_dicom_write(path, img, meta, const char* configJsonOrNull)` | `(path, img, meta)` — 3인자 | **시그니처 불일치** |
| api-spec:1258 | `xpe_dicom_write_j2k(path, img, meta, float compressionRatio)` | `(path, img, meta)` — 3인자 | **시그니처 불일치** |
| api-spec:1320 | `xpe_dicom_cstore(filePath, remoteAeTitle, remoteHost, remotePort, localAeTitle, …)` | `(host, port, aet, filePath, timeoutMs)` | **시그니처 불일치** (인자 순서·개수) |

**enhance_basic 다섯 건은 같은 형태다** — 문서가 전부 `const char* configJsonOrNull` 을
말하는데 실제는 타입 있는 파라미터 구조체다. 한 번에 생긴 오류로 보인다.

### 1.2 유령 — 실재하지 않는 함수 (7건)

| 문서 위치 | 문서가 말하는 것 | 실제 | 판정 |
|---|---|---|---|
| api-spec:1042 | `xpe_voi_lut_apply(img, wc, ww, function)` | 헤더·DLL 모두 없음 | **유령** |
| api-spec:1058 | `xpe_voi_lut_apply_fast(...)` | 없음 | **유령** |
| api-spec:1075 | `xpe_voi_lut_apply_sequence(...)` | 없음 | **유령** |
| api-spec:1196 | `xpe_dicom_read(path, imgOut, metaOut)` | 없음 | **유령** |
| api-spec:1211 | `xpe_dicom_query_dimensions(...)` | 없음 | **유령** |
| api-spec:1227 | `xpe_dicom_read_tag_string(...)` | 없음 | **유령** |
| api-spec:1274 | `xpe_dicom_set_tag_string(...)` | 없음 | **유령** |
| api-spec §12.2 | `gsvg_process_ex(...)` `GSVG_API`/`GsvgErrorCode` | 없음 | **유령** |
| api-spec §12.3 | `gsvg_version(void)` `GSVG_API` | 없음 (실제는 `xpe_gsvg_version`) | **유령** |
| api-spec §12.4 | `gsvg_error_string(GsvgErrorCode)` | 없음 | **유령** |

### 1.3 문서에서 아예 빠진 실제 export

| 모듈 | 문서에 없는 실제 export | 비고 |
|---|---|---|
| **display** | `xpe_apply_modality_lut`, `xpe_apply_voi_lut`, `xpe_apply_presentation_lut`, `xpe_voi_preset_create`, `xpe_gsdf_calibrate`, `xpe_display_version` — **6개 전부** | api-spec 에서 이름 검색 결과 **0건**. 대신 §1.2 의 유령 3개가 그 자리에 있다 |
| **dicom** | `xpe_dicom_open`, `xpe_dicom_close`, `xpe_dicom_read_image`, `xpe_dicom_get_metadata`, `xpe_dicom_cancel` — 10개 중 5개 | 각각 검색 결과 0건 |
| **gsvg** | `xpe_gsvg_init`, `xpe_gsvg_shutdown`, `xpe_gsvg_version` | 전용 절 없음(§12.1 본문에 이름만 언급) |

**display 는 문서화된 export 와 실재하는 export 의 교집합이 공집합이다.**
개수 표(§4)는 `6` 으로 맞는데, **맞는 것은 숫자뿐이다.**

### 1.4 모듈 문서(`docs/<module>/`)로의 전파 (`_ghost_census.log` 근거)

| 파일 | 유령 심볼 출현 |
|---|---|
| `docs/dicom/README.md` | `xpe_dicom_read(` ×7, `query_dimensions` ×1, `read_tag_string` ×1 |
| `docs/dicom/SAD-DICOM-001…md` | `xpe_dicom_read(` ×4 |
| `docs/dicom/SHA-DICOM-001…md` | ×2 |
| `docs/dicom/SRS-DICOM-001…md` | ×2 |
| `docs/display/SAD-DISPLAY-001…md` | `xpe_voi_lut_apply` ×3 |
| `docs/display/SRS-DISPLAY-001…md` | ×2 |
| `docs/display/xpe-display-prd.md` | ×4 |
| `docs/post-processing/gsvg/README.md` | `GsvgErrorCode` ×3, `GSVG_ERR_` ×8, `gsvg_error_string` ×3, `gsvg_version(` ×1 |
| `docs/post-processing/gsvg/GSVG-SDD-001…md` | 같은 계열 ×12 |
| `docs/post-processing/gsvg/GSVG_IEC62304…md` | 같은 계열 ×13 |
| `docs/post-processing/gsvg/GSVG-SVP-001…md` | ×2 |
| `docs/post-processing/xpe/XPE-SDD-002…md` | `xpe_dicom_read(` ×1 |
| `docs/project/production-integration-guide.md` | `xpe_dicom_read(` ×1 |
| `docs/project/sprint-plan.md` | 혼합 ×13 |

---

## 2. 정정문 우선 확인 (카드 2항)

"Corrected" / "정정" / "corrected" 표시가 붙은 문장을 **먼저** 대조했다.
이번 계열의 발단이 그 자리였기 때문이다.

| 위치 | 정정문이 주장하는 것 | 판정 |
|---|---|---|
| api-spec:140 | "GSVG uses `XPE_API` and `XpeErrorCode` from xpe_common (corrected 2026-09-11, QA-B-39)" | **참** — 다만 **같은 문서 §12.2~12.4 가 `GSVG_API`/`GsvgErrorCode` 로 이 정정을 정면으로 반박한다**. 정정이 §12.1 에만 적용됐다 |
| api-spec:187 | "`xpe_calc_exposure_index` moved to enhance_basic; count corrected 2026-09-11 (QA-B-39)" | **개수는 참(7), "moved" 는 거짓** — §3 참조 |
| api-spec:186 | "enhance_basic 8; includes `xpe_calc_exposure_index` moved from enhance_advanced" | 같은 거짓 |
| api-spec:188 | "ai 10; count corrected (QA-B-39)" | **참** |
| api-spec:189 | "display 6; count corrected (QA-B-39)" | **참(숫자만)** — 이름은 §1.3 대로 전부 어긋난다 |
| api-spec:191 | "gsvg 4; count corrected (QA-B-39)" | **참** |
| api-spec:1342 | cfind_mwl 인자 순서 정정 (QA-B-39) | **참** — 헤더와 일치 |
| api-spec:1380 | gsvg 정정 2건 (QA-B-39 → QA-B-53) | **§12.1 본문은 참**. 다만 그 **바로 위 코드 블록이 아직 #152 이전 시그니처**이고, §12.2~12.4 는 반박된 형태 그대로다 |

**요약: 정정문 8건 중 4건이 부분적으로 또는 전부 틀렸다.** 공통 형태가 하나 있다 —
**정정이 자기가 고친 자리에만 적용되고, 같은 주장을 담은 이웃 문장은 그대로 남았다.**
정정문은 "막 확인된 것" 처럼 읽히므로 그 이웃은 아무도 다시 보지 않는다.

---

## 3. 실제 쪽 결함 1건 — 문서 오류가 가리고 있던 것

"moved" 가 거짓인 것은 문서 문제이지만, **왜 거짓인지를 재 보니 코드 쪽 위험이 나왔다.**

### 3.1 같은 심볼을 두 DLL 이 내보낸다

`xpe_calc_exposure_index` 는 `xpe_enhance_basic.dll` 과 `xpe_enhance_advanced.dll` **양쪽**에
있고 시그니처는 인자 이름만 다르다. 두 헤더를 함께 포함하는 번역 단위가 실재한다
(`tests/e2e_post_pipeline/test_e2e_full_pipeline.cpp:21-22, 140`) — 컴파일도 링크도 통과하고,
**어느 쪽이 호출되는지는 링크 순서가 정한다.**

`dumpbin /imports`(`_imports.log`) 실측:

```
xpe_enhance_basic.dll -> 0 xpe_calc_exposure_index
```

**그 바이너리는 basic 쪽을 부른다. advanced 사본은 그 호출자에게 도달 불가다** —
설계가 아니라 링크 순서로.

### 3.2 두 구현은 같은 답을 내지 않는다 (`_dup.log`)

문서가 틀렸다는 것만으로는 위험의 크기를 모른다. 각 DLL 을 **이름으로 열어** 한
프로세스에서 둘 다 호출해, 링크 순서 문제를 우회하고 물었다:

| | 반환 | EI | DI |
|---|---|---|---|
| `xpe_enhance_basic.dll` | `0` | **200** | 0 |
| `xpe_enhance_advanced.dll` | `0` | **100000** | **26.0206** |

**같은 심볼, 같은 시그니처, 다른 임상 수치.** 전제 조건도 다르다 — advanced 사본은
`xpe_enhance_advanced_init` 전에 `NOT_INITIALIZED(-6)` 을 돌려주고, basic 사본은 그런 요구가
없다.

**대조군 규율이 여기서도 한 번 걸렀다.** 첫 측정에서 advanced 가 `-6` 을 돌려준 것을
"값이 다르다" 로 적을 뻔했는데, `init` 을 부르지 않은 것은 **구현의 차이가 아니라 테스트가
빠뜨린 단계**였다(B-52 에서 잘못된 credit 4건을 잡은 것과 같은 형태). 초기화 후 다시 쟀고,
그때도 다르다.

### 3.3 고치지 않았다

export 를 없애거나 두 알고리즘을 통일하는 것은 **API 결정**이다. `KnownDivergence_` 로
현행만 고정하고, 단언은 두 값이 아니라 **"둘이 다르다"** 에 걸었다 — 값은 정당하게 바뀔 수
있는 알고리즘 상수이고, 조용히 변하면 안 되는 것은 **답이 갈린다는 사실**이다.
**리더 결정 요청.**

---

## 4. Baseline 귀속 (Baseline-attribution)

| 항목 | 값 | 근거 |
|---|---|---|
| export 실측 | 6모듈 45개, 헤더와 100% 일치 | `_exports.log` + 헤더 추출 |
| 시그니처 불일치 | **8건** | `_audit_apispec.log` (기계 대조) |
| 유령 | **10건** | 같은 로그 + `_ghost_census.log` |
| 문서 누락 | display 6/6, dicom 5/10, gsvg 3/4 | §1.3 |
| 정정문 | 8건 중 **4건 부분/전부 오류** | §2 |
| import 실측 | e2e → `xpe_enhance_basic.dll` | `_imports.log` |
| 두 구현 값 | EI 200 vs 100000 | `_dup.log` (BUILD=0) |
| 이전 ctest | 474 / 211 / 173 | QA-B-53 `_verify.log` |
| 현재 ctest | **475 / 211 / 173** (신규 1건) | `_verify.log` |
| 빌드 경고 | 0 | `_verify.log` |
| 제품 코드 변경 | **0** | `git status` = 테스트 1개 + CMakeLists 1개 |

---

## 5. 문서 오류 목록 (leader 처리용)

`docs/`·`.moai/` 는 leader 소유이므로 **고치지 않았다.** 처리 순서 제안은 영향 크기 순이다.

1. **api-spec §11 dicom** — 유령 4개(`read`/`query_dimensions`/`read_tag_string`/`set_tag_string`) 제거, 실제 5개(`open`/`close`/`read_image`/`get_metadata`/`cancel`) 추가, `write`·`write_j2k`·`cstore` 시그니처 정정.
2. **api-spec display 절** — 유령 3개(`xpe_voi_lut_apply*`) 제거, 실제 6개 전부 추가. **현재 교집합이 공집합이다.**
3. **api-spec §12.2~12.4** — `gsvg_process_ex`/`gsvg_version`/`gsvg_error_string` 절 삭제. §12.1 위쪽 **낡은 코드 블록**(#152 이전)도 함께.
4. **api-spec §12.1·12.2 의 `GSVG_OK`/`GSVG_ERR_*` 오류 코드 줄** — 그런 enum 은 존재하지 않는다(`grep` 0건). §140 의 정정문과 모순.
5. **api-spec enhance_basic 5건** — `configJsonOrNull` → 실제 파라미터 구조체.
6. **api-spec §4 표의 "moved" 주석 2줄** — 옮겨지지 않았다. 두 DLL 이 함께 내보내며 **값이 다르다**(§3). 주석 정정과 별개로 **API 결정이 필요하다.**
7. **모듈 문서 14개 파일**(§1.4) — 같은 유령이 전파돼 있다.

---

## 6. 미검증 (Gaps)

- **preprocess·common 은 보지 않았다** — 다른 레인 소유(카드 제외). §1.4 census 에는
  경계상 `docs/project/*` 가 섞여 있으므로, 그 파일들의 preprocess 관련 항목은 이 표의
  대상이 아니다.
- **기계 대조는 `XPE_API`/`GSVG_API` 로 시작하는 코드 블록만 본다.** 산문 속 시그니처
  서술(예: 인자 이름만 언급)은 잡지 못한다.
- **반환 코드·에러 의미 서술은 대조하지 않았다.** 이름과 시그니처만 봤다.
- **SRS·SAD 의 요구 서술이 실제 동작과 맞는지는 보지 않았다** — 이 카드는 export 면이다.
- **두 `xpe_calc_exposure_index` 중 어느 쪽이 옳은지 판정하지 않았다.** 서로 다르다는
  것만 쟀다. 어느 값이 REQ-ENH-030 을 만족하는지는 별도 확인이 필요하다.
- **C# 호스트가 어느 DLL 을 로드하는지 보지 않았다.** e2e 바이너리 한 개만 쟀다.

---

## 7. 잔여 위험 (Residual-risk)

- **문서가 틀린 것보다 §3 이 위험하다.** 두 DLL 이 같은 이름으로 다른 임상 수치를 내고,
  선택은 링크 순서가 한다. 현재 e2e 는 basic 을 부르지만 **그 사실은 소스 어디에도 없고**,
  라이브러리 나열 순서가 바뀌면 조용히 바뀐다.
- **`KnownDivergence_` 는 위험을 없애지 않는다.** 표류를 막을 뿐이다.
- **문서 정정 자체가 위험 행위임이 이번에 다시 확인됐다**(§2). 정정할 때 **같은 주장을 담은
  이웃 문장을 함께 보지 않으면** 새 오류가 남고, 정정 표시 때문에 재확인되지 않는다.
- **이 감사도 한 번의 관측이다.** 기계 대조는 형태가 맞는 선언만 잡으므로, 잡히지 않은
  서술형 주장이 남아 있을 수 있다.
- 커밋은 push 전까지 미푸시 유일본이다.

---

## 부록 — 증거 파일

| 파일 | 내용 |
|---|---|
| `_env.bat` / `_verify.bat` | 환경 + 세 프리셋 재빌드·전체 ctest |
| `_dump.bat` / `_exports.log` | 6모듈 dumpbin export 목록 |
| `_audit_apispec.log` | 기계 대조 — 시그니처 불일치 8 · 유령 10 |
| `_imports.bat` / `_imports.log` | e2e 바이너리가 어느 DLL 에서 심볼을 가져오는가 |
| `_b54.bat` / `_dup.log` | 두 DLL 값 비교 — EI 200 vs 100000 (BUILD=0) |
| `_verify.log` | 최종 475 / 211 / 173, 경고 0 |
