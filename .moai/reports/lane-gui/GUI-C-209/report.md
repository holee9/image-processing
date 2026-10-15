# GUI-C-209 — #249 의 남은 시험 공백: D9 · D11 · D14 · D10 (REQ-061 설명만)

커밋: M1 `646c8f2f`, M2 `b39b34a9`, M3 은 이 보고서와 같은 커밋. 증거(이 폴더): `m1_falsification_arms.txt`, `m1_x86_native_dir_run.txt`, `m2_falsification_arms.txt`, `m3_gate_runs.txt`, `ci_ac12_step.ps1.txt`, `ci_ac12_gate.patch.txt`.
IntegrationTests 전체: **685 통과 / 0 실패 / 0 건너뜀**(전 662).

## M1 — D9 (REQ-GUI-IT-042) 다른 아키텍처 DLL 거부

요구 문구: "While the resolved DLL is x86 and the test host is x64 (or vice versa), the first P/Invoke call **shall raise `BadImageFormatException`** and the test **shall surface this as a test failure with the resolved path**."

**아주 작은 x86 PE 를 시험 자산으로 두는 안은 쓰지 않았다 — 실측으로 틀린 안임을 확인했다.** 손으로 만든 최소 PE32(섹션 0개)는 Machine 필드가 i386 이든 ARM64 이든 **AMD64 이든 전부** `BadImageFormatException`(0x8007000B)으로 거부된다(헤더가 불완전해서). 그런 자산은 "x86 이라서 거부"를 증명하지 못한다. 그래서 **실제 x64 시스템 DLL(`version.dll`)의 PE 헤더 Machine 한 필드만 바꾼 사본**을 시험 중에 만든다(저장소에 바이너리 없음). 바꾸지 않은 사본이 **대조군**(적재됨)이라 둘의 차이는 Machine 값뿐이다.

`ArchitectureMismatchTests`(3개): (1) 로더가 i386·ARM64 사본을 `BadImageFormatException`·HRESULT 0x8007000B 로 거부하고 부트스트랩 가드(`VerifyX64Pe`)가 로더와 같은 판정; (2) 첫 `[DllImport]` 호출이 던지는 예외(로더 직접 호출이 아니라 P/Invoke 호출); (3) `XPE_NATIVE_DIR` 의 x86 `xpe_common.dll` 이 로케이터에 잡히고 가드가 거부하며 진단이 **경로를 담는다**("…is not x64"). 마지막 요구절("test failure with the resolved path")은 `ResolvedDll_IsX64Architecture` 가 이미 `Assert.Fail` 로 하는 일이라, **x86 표시 DLL 을 native 폴더로 두고 전체 스위트를 실제로 돌린 출력**을 `m1_x86_native_dir_run.txt` 에 남겼다(실패 메시지에 경로 포함). 반증 5팔 전부 빨강(`m1_falsification_arms.txt`).
**한계**: 진짜 x86 빌드 DLL 로 한 번도 돌리지 않았다(이 기계에 x86 툴체인으로 빌드한 xpe_common 없음). "다른 아키텍처 = Machine 필드 다름"이라는 로더의 판정 근거는 같지만 x86 의 *내용*(32비트 코드)까지 본 것은 아니다. 반대 방향(x86 호스트가 x64 DLL)은 시험하지 않았다(`PlatformTarget=x64` 라 x86 호스트가 없다).

## M2 — D11 (REQ-006·050·052, AC-9 "20+") 서로 다른 거부 경로

**결론: xpe_common 의 서로 다른 거부 경로는 18개이고, "20+" 는 한 검사를 두 번 세어야 채워진다.** 새 `NegativeInputPathTests` 는 네이티브 소스의 검증문(`if`)마다 한 행이다. 표(행 = 입력 → 정확한 코드 → 지나는 검사):

| # | 입력 | 정확한 코드 | 지나는 검사 (소스) |
|---|---|---|---|
| 1 | `xpe_init("")` | CONFIG_INVALID | `configJsonOrNull[0] == '\0'` (xpe_common.cpp) |
| 2 | `xpe_configure(NULL)` · `("")` | INVALID_INPUT | `!jsonConfig \|\| jsonConfig[0] == '\0'` |
| 3 | `xpe_configure("{\"a\":")` | CONFIG_INVALID | `nlohmann::json::accept` |
| 4 | `xpe_configure("[1,2]")` | CONFIG_INVALID | `*p != '{'` |
| 5 | `xpe_get_param_range(NULL,…)` | INVALID_INPUT | `!bodyPart \|\| !paramName` |
| 6 | 같은 호출, `xpe_shutdown()` 뒤 | NOT_INITIALIZED | `!g_initialized` |
| 7 | `("KNEE","gamma")` | INVALID_INPUT | `!validBodyPart` |
| 8 | `xpe_get_pending_alert(0, NULL, …)` | INVALID_INPUT | `!msg \|\| msgLen == 0 \|\| !severity \|\| index < 0` |
| 9 | 같은 호출, 빈 큐에 index 0 | INVALID_INPUT | `index >= g_alertQueue.size()` |
| 10 | 6자 알림에 버퍼 3 | BUFFER_TOO_SMALL | `e.message.size() + 1 > msgLen` |
| 11 | `xpe_log_set_level(-1)` · `(6)` | INVALID_INPUT | `level < 0 \|\| level > 5` (xpe_logging.cpp) |
| 12 | `xpe_log_set_file(없는 폴더/x.log)` | IO_FAILED | `!std::filesystem::exists(parent)` |
| 13 | `xpe_alloc_image(0,16,…)` · `out == NULL` | INVALID_INPUT | `!out \|\| width == 0 \|\| height == 0` (xpe_memory.cpp) |
| 14 | `xpe_alloc_image(4097,1,…)` | INVALID_INPUT | `width > 4096 \|\| height > 4096` |
| 15 | `xpe_alloc_image(16,16,(형식)99)` | UNSUPPORTED_FORMAT | `!bytes_per_pixel(format,…)` |
| 16 | `xpe_free_image(NULL)` | INVALID_INPUT | `if (!buf)` |
| 17 | `xpe_copy_image(data 없는 src, dst)` | INVALID_INPUT | `!src \|\| !dst \|\| !src->data \|\| !dst->data` |
| 18 | `xpe_copy_image(8x8 → 4x4)` | BUFFER_TOO_SMALL | `dst->dataSize < src->dataSize` |

같은 `if` 에 다른 입력(NULL 대 빈 문자열, -1 대 6, 0 대 NULL out)을 넣은 것은 **한 경로의 두 변형**이라 한 행 안에서 둘 다 단언하고 두 번 세지 않는다.

각 행에는 **유효 입력 대조군**이 있다(같은 호출을 유효한 값으로 하면 OK) — 거부가 "초기화 안 됨" 같은 다른 이유 때문이 아님을 붙잡는다. 표가 낡지 않도록 `EveryRow_CitesACheckThatExistsInTheNativeSource_…` 가 인용한 검사 문장이 네이티브 소스에 **실재하는지**(소스를 읽어서)와 **같은 검사를 두 행이 인용하지 않는지**를 단언하고, 소스 검색이 실제로 읽는지의 대조 시험이 따른다. **managed 마샬러는 NULL `out`/`ref` 를 만들 수 없어**(13의 NULL out, 16의 NULL buffer) 그 두 입력은 함수 포인터로 호출한다(거울의 `[DllImport]` 만 쓰는 시험은 이 두 입력을 만들 수 없다. 다른 방식으로 이 두 경로를 지나는 시험이 있는지는 검색하지 않았다).
**기존 시험과의 차이**: `NativeErrorTranslationTests` 는 "BUFFER_TOO_SMALL 또는 INVALID_INPUT"처럼 **둘 중 하나**를 받는 행이 있어 REQ-052 의 "documented expected code" 를 못 지킨다. 이 클래스는 행마다 정확히 하나다. 기존 시험은 그대로 두었다(손대지 않음).
**반증 24팔(`m2_falsification_arms.txt`)**: 18개 행 각각의 입력을 유효한 값으로 바꾸면 그 행이 빨강(13 은 두 변형 각각), 기대 코드를 바꾸면 빨강, 대조군을 잘못된 호출로 바꾸면 빨강, 표 쪽 셋(같은 검사 이중 인용, 소스에 없는 인용, 행 삭제)도 빨강. 팔마다 빨강은 **그 팔이 겨눈 행 1개**뿐이다. 전부 바이트 동일 복원.
**한계**: 네이티브 DLL 을 다시 빌드하지 않았다(검사를 *지우는* 네이티브 변이는 CI 몫이다 — 시험은 "이 입력이 이 코드를 낸다"를 단언하지 "코드가 그 줄에서 나온다"는 소스 인용으로만 이어진다). xpe_common 밖의 DLL(preprocess·enhance·display·dicom·ai)에도 거부 경로가 있으나 이 요구의 범위(xpe_common)가 아니라 세지 않았다.

## M3 — D14 (AC-12) 시간 상한을 실제로 재기

SPEC §7: Smoke < 30 s, 전체 < 2 min, 시험 하나 < 5 s, 1000회 누수 시험(`Category=Safety`) < 90 s, "CI 에서는 ×1.5 여유 권장". **지금 이것을 재는 곳은 없다** — CI 의 `dotnet-tests` 는 필터 없이 전체를 한 번 돌리므로 Smoke 는 단독으로 선택된 적이 없고, 어떤 소요 시간도 한계와 비교되지 않는다(207 에서 `Category=Smoke` 가 0이라 한 것은 그 시점 사실이었고, 지금은 5개 클래스·19개 시험이 선택된다). 이 기계의 측정: 전체 11.6 s, Smoke 19개 2 s, Safety 10개 1–3 s, 가장 느린 시험 2.68 s.

`ci.yml` 은 리더 소유라 **패치 초안**을 낸다: `ci_ac12_gate.patch.txt`(`git apply --check` 통과, YAML 로드 확인, 스텝 본문이 `ci_ac12_step.ps1.txt` 와 왕복 동일). 새 스텝 "Assert the SPEC time gates (AC-12)" 를 `Run C# integration tests` 바로 뒤에 둔다: trx 의 `start..finish` 로 전체, 가장 느린 단일 시험, 그리고 `Category=Smoke`·`Category=Safety` 를 따로 돌려 프로세스 시작부터의 wall-clock 을 한계(×1.5: 45 s / 180 s / 7.5 s / 135 s)와 비교한다. **선택된 시험 수가 기대보다 적으면 실패**(아무것도 못 고른 필터는 즉시 끝나 "빠른 통과"가 된다; Smoke 최소 10). 실패·건너뜀 수도 본다.
**로컬 실증(`m3_gate_runs.txt`)**: 실제 trx 로 기본값은 통과, 한계를 내리면(Smoke 1 s, Safety 1 s, 전체 5 s, 시험 0.5 s) 각각 빨강, Smoke 최소를 1000 으로 올리면 빨강, 없는 trx 도 빨강. 스크립트는 ASCII 만 쓰고 요약 줄을 위치로 읽는다(처음 한글 요약 단어를 정규식에 넣었다가 PowerShell 5 가 BOM 없는 UTF-8 을 잘못 읽어 깨졌고, 위치 파싱으로 바꿨다 — `feedback_local_lint_reads_differently_than_ci` 와 같은 원인).
**한계**: CI 러너의 실제 시간은 모른다(이 기계 기준 ×1.5). 러너가 로컬보다 느리면 한계에 가까워질 수 있으니 첫 CI 실행의 출력(`full run: … s`, `Category=Smoke: … s`)을 읽고 한계를 정해야 한다. 한계 초과로 처음 빨강이 되면 코드가 아니라 한계 선택이 원인일 수 있다. 스텝은 `Debug` 빌드가 이미 있다고 가정한다(`--no-build`).

## D10 — REQ-061 (입력 SHA-256 보존) 코드 변경 없이, 리더가 결정

**요구 문구**: "**Where** `xpe_preprocess.dll` exports `xpe_offset_correct`, `xpe_gain_correct`, `xpe_defect_correct` simultaneously, the test suite **shall** run a 16x16 synthetic offset→gain→defect chain (equivalent to `XpePreprocessSyntheticOracle.RunChain`) and assert: input SHA-256 preserved, output NaN/Inf count = 0, determinism RMSE = 0 across two consecutive runs." 참조: 그 오라클을 "xUnit Fact 로 승격".

**지금 동작(읽어서 확인)**: 네 속성을 모두 계산하는 것은 레거시 앱의 `clients/ImageProcTest/Diagnostics/XpePreprocessSyntheticOracle.cs`(`RunChain` 두 번, 입력 SHA 전/후, NaN/Inf, RMSE)이고 그것은 **xUnit 이 아니라 진단 코드**다(앱의 준비도 프로브가 부른다). `IntegrationTests` 의 `PreprocessCorrectionChainSmokeTests` 는 결정성(RMSE 0)과 NaN/Inf 없음을 단언하지만 그 사슬은 **offset→gain 두 단계**이고(`RunCalibratedChain`: `xpe_offset_correct`, `xpe_gain_correct`) `xpe_defect_correct` 단계가 없으며 **입력 SHA-256 보존은 단언하지 않는다**. 입력 SHA 보존을 단언하는 시험은 다른 경로(`BaselineDeterminismTests`, 베이스라인 파이프라인)에 있다. 즉 REQ-061 의 "defect 단계 포함"과 "입력 SHA 보존" 두 조각이 xUnit 에 없다.
**구현한다면 바뀌는 것**: `PreprocessCorrectionChainSmokeTests` 에 세 번째 단계(`xpe_defect_correct`, 마스크 버퍼 필요)를 더하고, 사슬을 부르기 전후의 입력 버퍼를 SHA-256 으로 비교하는 단언 한 개를 더한다 — 시험 파일 하나, 네이티브 변경 없음. 위험: defect 단계의 마스크 계약(`XCAL_FMT_UINT8_MASK`, 교정 맵 적재 요구)에 따라 "교정 없음" 사슬이 `CALIB_NOT_LOADED` 를 낼 수 있어, 오라클이 쓰는 "identity/no-calibration semantics" 와 다른 설정이 필요할 수 있다(내가 확인하지 않은 부분). 결정은 리더 몫.

## 리더 메시지(QA-A-229 M4) 관련
새 경고 `XPE_WARN_CALIB_SESSION_UNSPECIFIED` 와 서로 다른 세션 교정 맵의 `CONFIG_INVALID` 는 **아직 main 에 없어** 이 카드 중에는 확인하지 못했다. 병합 뒤 gui 시험 생성기(`GenerateAndLoadCalibration`, 세션 없는 파일)가 경고만 내고 거부되지 않는지 확인이 필요하다 — 다음 카드나 병합 알림으로 하겠다.
