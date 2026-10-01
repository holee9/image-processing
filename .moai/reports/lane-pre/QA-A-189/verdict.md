# QA-A-189 (#216) — 수출 C ABI 48개와 SPEC-XPE-P1A 요구 본문 (오늘 상태, 보고만)

시작 HEAD `ef98cd67` (`evidence/00_head.txt`). 코드·문서 변경 없음. 이 카드가 추가한 것은 이 보고서와 증거 파일뿐이다.

## 한눈에

| 칸 | 개수 |
|---|---|
| 이름 일치 (SPEC 요구 본문이 그 함수의 동작을 이름으로 요구) | 22 |
| 동작만 일치 (이름은 없으나 SPEC 요구가 그 동작을 요구) | 1 |
| SRS 에만 있음 (SPEC 에는 없고 SRS-CALIB-001 이 이름 또는 능력으로 요구) | 20 |
| 요구 없음 (SPEC 에도 SRS 에도 없음; 이름이 지나가는 언급으로만 나옴) | 5 |
| 합계 | **48** (= `dumpbin` 이 본 수출 53개 중 C++ 맹글 이름 5개를 뺀 수) |

**이슈 제목의 "21개가 REQ-P1A 요구에 이름이 없다"는 오늘도 이름 축으로는 맞다.** 그러나 이름 축과 행위 축이 같지 않다 (§③). 그리고 21개 중 20개는 이미 SRS-CALIB-001 에 요구가 있다 — SPEC 이 아니라 SRS 에 산다.

## 방법과 대조군

- 수출 집합: `evidence/01_dumpbin_exports.txt` 에서 53개, 맹글 이름(`?…`) 5개(`read_xcal_file` 등 C++ 도우미)를 제외한 C ABI 이름 48개 (`02_`, `03_`). 이 DLL 은 HEAD `ef98cd67` 의 트리에서 빌드된 것이다 (제품 소스 변경 없음).
- **이름 축**: SPEC-XPE-P1A `spec.md` 를 `#### REQ-P1A-…` 헤더로 쪼개 요구 43개의 **본문**(헤더 제목 줄 제외)에서 함수 이름을 낱말 경계로 찾았다 (`05_name_census.txt`, `04_name_census.json`). 대조군: 요구 43개 파싱, 본문 712줄 — 정의된 요구 수(001–005, 010–013, 014–019/016a, 020/020a/021/022, 030–033, 040–042, 080–082, 085–088, 090–091, 095–101 = 43)와 같다.
- **행위 축**: 각 요구의 `shall` 절을 읽고, 그 절이 함수가 하는 일을 요구하는지를 사람이 판정했다. 이름이 나와도 `shall` 이 그 함수의 동작이 아니면 "지나가는 언급"으로 뺐다.
- **헤더 축(기계)**: 공개 헤더 `preprocess_api.h` 의 각 선언 바로 위 문서 블록이 인용하는 `REQ-P1A-…` 를 뽑아 이름 축과 대조 (`08_header_axis.txt`, 대조군: 선언 48/48 발견).
- 이름 매칭은 `grep -w` 가 아니라 정규식 낱말 경계(`(?<![A-Za-z0-9_])이름(?![A-Za-z0-9_])`)로 직접 썼다 (QA-A-187 에서 `-w` 가 접미사 이름을 놓친 일 때문). 양성 대조군: `xpe_preprocess_init` 이 `REQ-P1A-001` 에 잡힘 (`05_name_census.txt`). 지어낸 이름으로 하는 음성 대조군은 이번에 돌리지 않았다 (Gaps 에 적음).
- 호출자 census 의 첫 시도는 경로 필터가 `ImageProcTest/`(제품 폴더)를 시험 폴더로 오인해 GUI·클라이언트 호출을 통째로 놓쳤다. 대조군(`xpe_preprocess_init` 의 GUI 호출이 0건으로 나옴)으로 잡고 경로 조각 단위로 고쳐 다시 셌다 (`07_nontest_refs.txt`).

## ① 48개 표

범례: **이름**=이름 일치, **동작**=동작만 일치, **SRS**=SRS 에만 있음, **없음**=요구 없음. `spec:N` 은 `spec.md` 줄, `SRS:N` 은 `SRS-CALIB-001` 줄, `RTM:N` 은 `RTM-CALIB-001` 줄.

| # | 함수 | 칸 | 근거 줄 |
|---|---|---|---|
| 1 | xpe_preprocess_init | 이름 | REQ-P1A-001 spec:101 ("initialize its internal state when `xpe_preprocess_init()` is called") |
| 2 | xpe_preprocess_shutdown | 이름 | REQ-P1A-020 spec:616 (종료 뒤 처리 함수가 `NOT_INITIALIZED` 를 돌려줌) — 종료 자체의 동작(해제)은 서술 없음, 사후 상태만 |
| 3 | xpe_offset_correct | 이름 | REQ-P1A-010 spec:140, REQ-020a spec:623 |
| 4 | xpe_gain_correct | 이름 | REQ-P1A-011 spec:154, REQ-020a spec:623 |
| 5 | xpe_defect_correct | 이름 | REQ-P1A-012 spec:169, REQ-020a spec:623 |
| 6 | xpe_defect_detect_runtime | 이름 | REQ-P1A-013 spec:266 |
| 7 | xpe_calib_load_offset | 이름 | REQ-P1A-014 spec:565 |
| 8 | xpe_calib_load_gain | 이름 | REQ-P1A-015 spec:572 |
| 9 | xpe_calib_load_defect_map | 이름 | REQ-P1A-016 spec:579 |
| 10 | xpe_calib_state_load | 이름 | REQ-P1A-016a spec:586 |
| 11 | xpe_calib_generate_offset | 이름 | REQ-P1A-017 spec:593 |
| 12 | xpe_calib_check_expiry | 이름 | REQ-P1A-018 spec:600 |
| 13 | xpe_calib_save | 이름 | REQ-P1A-019 spec:607 |
| 14 | xpe_temp_compensate | 이름 | REQ-P1A-080 spec:655, REQ-082 spec:674 |
| 15 | xpe_ghost_create | 이름 | REQ-P1A-085 spec:680 |
| 16 | xpe_ghost_destroy | 이름 | REQ-P1A-085 spec:680, REQ-086 spec:688 |
| 17 | xpe_ghost_correct | 이름 | REQ-P1A-087 spec:696 (+REQ-086·099) |
| 18 | xpe_ghost_reset | 이름 | REQ-P1A-088 spec:703 |
| 19 | xpe_binning_correct | 이름 | REQ-P1A-090 spec:712 |
| 20 | xpe_preprocess_pipeline | 이름 | REQ-P1A-095 spec:739 |
| 21 | xpe_validate_readout_artifact | 이름 | REQ-P1A-041 spec:862 (선 잡음 부분은 미구현, #232) |
| 22 | xpe_preprocess_get_param_range | 이름 | REQ-P1A-042 spec:871 |
| 23 | xpe_nonlinearity_correct | **동작** | 이름 없음. REQ-P1A-095 spec:741 (단계 순서에 비선형 단계)·REQ-096 spec:748 (`NONLINEARITY_CORRECTED` 플래그)가 그 단계를 요구. 이 함수는 `xpe_nonlinearity_apply` 를 감싼 얇은 래퍼(`nonlinearity_correct.cpp:365`)이고 파이프라인 단계가 쓰는 것이 `xpe_nonlinearity_apply`(`pipeline.cpp:177`)다. SRS:55 (FUNC-006)·RTM:246 이 이름으로 매핑 |
| 24 | xpe_calib_generate_gain | SRS | SPEC 은 REQ-P1A-011 spec:158 에서 "`xpe_calib_generate_gain` 이 쓰는 값"으로 **지나가며 언급**만 한다. 생성 동작은 SRS:181 (FUNC-026) |
| 25 | xpe_calib_generate_gain_polynomial | SRS | SRS:182 (FUNC-027)가 이름을 직접 부름 |
| 26 | xpe_calib_get_mode | SRS | SRS:221 (FUNC-031)가 이름을 직접 부름 |
| 27 | xpe_calib_set_mode | SRS | SRS:221 (FUNC-031) |
| 28 | xpe_calib_get_max_points | SRS | SRS:469 (FUNC-038)가 이름으로 묶음. 값 자체는 FUNC-031 (SRS:221) |
| 29 | xpe_calib_get_poly_degree | SRS | SRS:469 (FUNC-038). 값은 FUNC-031 |
| 30 | xpe_calib_get_quality_meta | SRS | SRS:469 (FUNC-038), 능력은 FUNC-033 (SRS:240, 이름 없이 서술) |
| 31 | xpe_calib_cache_clear | SRS | SRS:469 (FUNC-038) |
| 32 | xpe_calib_cache_set_max_size | SRS | SRS:469 (FUNC-038) |
| 33 | xpe_calib_state_release | SRS | SRS:469 (FUNC-038). SPEC REQ-016a 는 `state_load` 만 요구 |
| 34 | xpe_preprocess_is_initialized | SRS | SRS:469 (FUNC-038). REQ-020 은 "초기화 전" 상태를 정의하나 조회 함수의 계약은 서술하지 않음 |
| 35 | xpe_preprocess_pipeline_batch | SRS | SRS:469 (FUNC-038). REQ-101 spec:808 이 스스로 "`_batch` 는 어떤 요구도 부르지 않는다"고 기록 |
| 36 | xpe_bpm_generate | SRS | 이름은 SRS 에 없고 능력은 SRS:162–165 (FUNC-022..025). RTM:244 가 이름으로 매핑 |
| 37 | xpe_calib_generate_nonlin_lut | SRS | 능력 SRS:66 (FUNC-006-EXT "LUT generation"). RTM:246 이 이름으로 매핑 |
| 38 | xpe_calib_load_nonlin_lut | SRS | SRS FUNC-006-EXT 6a (SRS:59), RTM:246. 적재를 함의만 함 |
| 39 | xpe_calib_unload_nonlin_lut | SRS | RTM:246 (매핑). FUNC-006-EXT 는 해제를 함의조차 안 함 |
| 40 | xpe_verify_offset | SRS | SRS:467 (FUNC-036)가 이름으로 부름. 능력 SRS:147 (FUNC-016), RTM:240 |
| 41 | xpe_verify_gain | SRS | SRS:467 (FUNC-036)가 이름으로 부름. 능력 SRS:148 (FUNC-017), RTM:241 |
| 42 | xpe_verify_defect | SRS | 이름은 SRS 에 없음. 능력 SRS:150 (FUNC-019) + FUNC-036 ("모든 `xpe_verify_*`"), RTM:242 가 이름으로 매핑 |
| 43 | xpe_verify_pipeline | SRS | 이름은 SRS 에 없음. 능력 SRS:146·152 (FUNC-015·021) + FUNC-036, RTM:243 가 이름으로 매핑 |
| 44 | xpe_calib_load_offset_cached | 없음 | REQ-P1A-014 spec:565 가 "caller-returning form" 으로 이름만 언급(요구문 아님). SRS 어디에도 이름·능력 없음 |
| 45 | xpe_calib_load_gain_cached | 없음 | REQ-P1A-015 spec:572 (지나가는 언급). SRS 없음 |
| 46 | xpe_calib_load_defect_cached | 없음 | REQ-P1A-016 spec:579 (지나가는 언급). SRS 없음 |
| 47 | xpe_preprocess_pipeline_ex | 없음 | REQ-P1A-016a spec:586 은 캘리브 상태 계약일 뿐, REQ-101 spec:808 이 "`_ex` 는 아직 서술하지 않았다"고 기록. SRS 없음 |
| 48 | xpe_preprocess_version | 없음 | SPEC·SRS 어디에도 이름·능력 없음 |

### 칸별 합계 점검

22 + 1 + 20 + 5 = 48. `SRS 에만 있음` 20 = 표 24~43. 이 중 **14개는 SRS 본문이 함수 이름을 직접 부르고**(24~35 열두 개, 40·41) **6개는 SRS 에 이름이 없고 능력으로만 서술**한다(36 bpm_generate, 37~39 비선형 LUT 셋, 42·43 verify_defect/pipeline — 이 여섯은 RTM 이 이름으로 매핑). SRS 이름 직접 호출 수는 `05_name_census.txt` 의 `srs(...)` 열로 셌다.

## ② "요구 없음/SRS 에만 있음" 함수가 제품에서 호출되는가

"제품" = `gui/`·`clients/`(앱 코드, 시험 폴더 제외)·파이프라인 내부. 호출은 `evidence/07_nontest_refs.txt` 와 `06_nontest_refs.json` 에서 정의·주석·선언을 제외하고 읽었다.

### 제품에서 호출됨 (요구 없음/동작만/SRS-only 중)

| 함수 | 칸 | 호출처 | 뜻 |
|---|---|---|---|
| xpe_nonlinearity_correct | 동작 | `gui/ImageProcTest/Services/Native/GuiPreprocessRunner.cs:143` | GUI 보정 경로가 호출. 그 동작은 SPEC 에서 파이프라인 **단계**로만 요구되고 이 공개 함수의 계약은 SPEC 에 없음 |
| xpe_preprocess_version | 없음 | `clients/ImageProcTest/Diagnostics/XpePreprocessReadinessProbe.cs:13,74` (`TryGetExport`) | 클라이언트 진단 프로브가 이름으로 읽음. 어느 문서에도 없는 함수를 제품 진단이 의존 |
| xpe_preprocess_is_initialized | SRS | `offset_correct.cpp:178`, `gain_correct.cpp:282`, `defect_correct.cpp:171` | 보정 함수 안의 `NOT_INITIALIZED` 가드(REQ-P1A-020) 구현이 이 함수를 부름. 요구가 정한 행동을 이 함수가 수행 |
| xpe_calib_get_mode | SRS | `xpe_calib_generate_gain.cpp:204,326,432,688` | 게인 생성 내부(FUNC-031 이 요구) |
| xpe_bpm_generate | SRS | `modules/preprocess/tools/xpe_calib_fixture_gen.cpp:531` | 모듈 개발 도구(제품 앱 아님) |
| xpe_calib_generate_gain / _polynomial | SRS | `tools/xpe_calib_fixture_gen.cpp:407,462,491` | 모듈 개발 도구 |

### 제품 코드에서 호출되지 않음 (시험에서만 쓰임)

`xpe_calib_cache_clear` · `xpe_calib_cache_set_max_size` · `xpe_calib_get_max_points` · `xpe_calib_get_poly_degree` · `xpe_calib_get_quality_meta` · `xpe_calib_generate_nonlin_lut` · `xpe_calib_load_nonlin_lut` · `xpe_calib_unload_nonlin_lut` · `xpe_calib_set_mode` · `xpe_calib_state_release` · `xpe_preprocess_pipeline_batch` · `xpe_preprocess_pipeline_ex` · `xpe_calib_load_offset_cached` · `xpe_calib_load_gain_cached` · `xpe_calib_load_defect_cached` · `xpe_verify_offset` · `xpe_verify_gain` · `xpe_verify_defect` · `xpe_verify_pipeline` (19개).

이 19개 모두 시험 파일은 1~7개씩 있다 (`09_test_refs_for_zero_caller_exports.txt`). GUI 는 지표를 `xpe_verify_*` 가 아니라 자기 C# 코드(`MetricsComputationService.cs`)로 계산한다 — `xpe_verify_*` 네 함수는 어느 제품 경로도 부르지 않는다.

즉 "요구 없음인 함수가 제품 경로에 있다"는 오늘 두 함수뿐이다: GUI 가 부르는 `xpe_nonlinearity_correct`(동작은 요구됨, 계약은 없음)와 클라이언트 진단이 읽는 `xpe_preprocess_version`(요구 없음). 나머지 "요구 없음" 넷(`*_cached` 셋, `_ex`)은 제품 호출자가 없다.

## ③ 이름 축과 행위 축 대조

| 축 | SPEC 이 다룬다 | 안 다룬다 |
|---|---|---|
| 이름 축 (본문에 이름이 있음) | 27 | 21 |
| 행위 축 (`shall` 절이 그 동작을 요구) | 23 | 25 |

두 축이 **같지 않다**. 차이는 정확히 6개 함수다.

- 이름은 있으나 행위 아님 (27 → 빠지는 5): `xpe_calib_generate_gain` (REQ-011:158 설명 문장), `xpe_calib_load_offset_cached` / `_gain_cached` / `_defect_cached` (REQ-014·015·016 의 "caller-returning form" 서술), `xpe_preprocess_pipeline_ex` (REQ-016a:586 은 상태 계약, REQ-101:808 이 스스로 미서술이라 기록).
- 이름은 없으나 행위 있음 (21 → 들어오는 1): `xpe_nonlinearity_correct`.

27 − 5 + 1 = 23. 이름으로만 세면 SPEC 의 빈틈이 5개 적게(= 21), 행위로 세면 25개다. **이슈 제목의 21은 이름 축이고, 행위 축의 SPEC 공백은 25개**다. 그러나 공백 25개 중 20개는 SRS 가 이미 요구하므로 요구가 아예 없는 것은 5개다.

### 세 번째 축(기계): 공개 헤더가 인용하는 REQ

`08_header_axis.txt`: 헤더 문서 블록이 인용하는 `REQ-P1A` 와 본문이 이름을 부르는 REQ 가 다른 함수가 25개로 나왔다. 대부분은 횡단 요구(REQ-003/005/020/021/022/030/031 등 모든 함수에 걸리는 요구)이거나, 이전 카드(QA-A-176~183)가 "옛 번호를 인용하지 않는다"고 적은 부정문을 내 정규식이 인용으로 센 것이다 (`xpe_nonlinearity_correct` 와 `xpe_verify_pipeline` — 둘의 블록이 "…REQ-P1A-012..015 this block used to cite…" 라고 부정문으로 적음; 오탐).

**진짜 어긋남 하나가 남는다**: `xpe_verify_offset`·`_gain`·`_defect` 헤더가 각각 "`SRS-CALIB-FUNC-016 / REQ-P1A-010: Offset correction verification`"(`preprocess_api.h:1021`, `:1044`, `:1066`), `…017 / REQ-P1A-011: Gain correction verification`, `…019 / REQ-P1A-012: Defect correction verification` 이라고 적는다. REQ-010/011/012 는 **보정 실행**(`xpe_offset_correct` 등)을 요구하지 `xpe_verify_*` 를 요구하지 않는다 (spec:140·154·169 본문에 `xpe_verify` 없음). `xpe_verify_pipeline` 블록은 이미 "No REQ-P1A- requirement covers xpe_verify_* (spec.md does not mention them)" 라고 고쳐 적혀 있으나 셋은 그대로다. 이름 축과 행위 축 모두 이 셋이 SPEC 에 없다고 말하므로 일치하지만 헤더만 반대로 인용한다.

## ④ 오늘 상태에서 바뀐 것 (이번 세션의 정정이 영향을 준 부분)

- `xpe_crc32` 수출 철회(`498fc3c`, #216 코멘트)로 수출이 49 → 48 이 됐고 이 표는 48 기준이다.
- #216 의 이전 정리(`e05d1661`: "매핑 누락 4, 묶는 요구 8" — RTM 에 비선형 넷 매핑, SRS `FUNC-038` 로 경계·수명 8종 묶음)가 이미 SRS/RTM 쪽에 들어가 있어 표 24~43 의 SRS 근거로 쓰였다.
- QA-A-176~188 의 정정은 SPEC 본문에서 함수를 새로 이름 짓거나 거두지 않았다 (이 census 의 이름 축 27/21 은 QA-A-151 시점의 "21" 과 같은 수).

## ⑤ 결정 재료 (한 줄씩)

1. **요구 없음 5개** (`*_cached` 셋, `pipeline_ex`, `preprocess_version`) — 결정에 필요한 것: 각각 (a) SPEC 에 요구를 세울지, (b) SRS FUNC-038 같은 묶음 요구에 넣을지, (c) 공개 수출에서 거둘지. `_cached` 셋과 `_ex` 는 제품 호출자가 없고, `preprocess_version` 은 클라이언트 진단이 읽는다.
2. **`xpe_nonlinearity_correct`** (동작만 일치, GUI 가 호출) — 결정에 필요한 것: 이 공개 함수의 계약(LUT 없을 때 no-op + 알림, `panel.linear` 해석)을 SPEC 요구로 세울지. 지금은 파이프라인 단계(REQ-095/096)로만 요구된다.
3. **SRS 에만 있는 20개** — 결정에 필요한 것: 이 20개를 SPEC-XPE-P1A 가 다룰 범위로 볼지(그러면 SPEC 에 요구 추가), SRS 만으로 충분하다고 볼지(그러면 RTM 매핑으로 닫고 이슈 제목의 "21"을 "SPEC 공백 20, 요구 없음 5"로 정정). 이 중 19개는 제품 호출자가 없다.
4. **`xpe_verify_offset/_gain/_defect` 헤더 인용** — 결정에 필요한 것: 헤더의 "REQ-P1A-010/011/012: … verification" 인용을 `xpe_verify_pipeline` 블록처럼 "SPEC 요구 없음, SRS FUNC-016/017/019" 로 고칠지 (주석만, 이번 카드에서는 안 고침).

## Gaps / Residual-risk

- 행위 축은 사람이 `shall` 절을 읽은 판정이라 기계 축이 아니다. 판정이 갈릴 수 있는 경계: `xpe_preprocess_shutdown`(사후 상태만 서술, 이름 일치로 둠), `xpe_preprocess_is_initialized`(REQ-020 이 상태를 정의, SRS 에만으로 둠), `xpe_calib_generate_gain`(지나가는 언급, SRS 에만으로 둠). 이 셋을 다르게 판정하면 칸 합계가 1~3 움직인다.
- 호출자 census 는 `gui/`·`clients/`·`tools/`·`modules/` 의 `.cpp/.h/.cs/.xaml/.py/.ps1` 만 봤다. 빌드 산출물·문서·다른 저장소(CI 스크립트가 쓰는 외부 도구)는 보지 않았다. `clients/ImageProcTest.E2ETests` 는 시험 폴더로 취급해 제외했으나 필터에는 `.E2E` 만 걸려 있어 그 폴더의 호출이 일부 섞였을 수 있다 (표의 제품 호출 목록은 수작업으로 걸러 확인).
- 이름 매칭의 음성 대조군(지어낸 함수 이름이 0건으로 나오는지)은 돌리지 않았다. 양성 대조군(`xpe_preprocess_init` 이 잡힘)과 요구 43개 파싱만 확인했다.
- `SRS-CALIB-001` 매칭은 이름 + 능력(FUNC 번호) 을 사람이 대조한 것이다 (QA-A-151 이 밝힌 대로 SRS 는 능력으로 쓰여 심볼 grep 은 구조적으로 눈멀다).
