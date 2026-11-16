리더 수용·커밋 준비 — PRE d3216b2a, 전체 E2E 369=345P/0F/24S; Integration 1022=1019P/0F/3S, 7프로젝트 빌드 성공

## 2026-11-17 01:05 리더 판정·커밋 범위

`20261117-0105-gui.md`에서 리더는 결과와 첫 스테이징 실패 원문 보존을 수용했다. 현재 dev/gui HEAD3d70e4e8 위에 이번 diff만 명시 경로로 커밋하도록 지정했고, 원격/main 차이35/1은 리더가 main 병합 때 정리한다. 따라서 아래 커밋 보류 사유는 해소됐다. GUI 레인은 main을 merge하거나 push하지 않는다. 커밋 직전 소스434파일을 다시 해시 대조해 기존 실행 기준과 불일치0건을 확인했다. 이는 시험 재실행이 아니라 기존 실행 소스와 현재 소스의 동일성 확인이다.

소스26경로와 이번 report.md/BLOCKED.md만 명시 경로로 stage했다. 원문 로그의 줄끝·공백 때문에 처음 cached check가 실패해 해당 로그는 index에서만 제외했다. 원문 파일은 변경하지 않고 작업 폴더에 보존한다. 최종 `git diff --cached --check`는 exit0·출력 없음이었다. 로그 정규화·검증 우회는 하지 않았다.

## 이전 커밋 전 상태 확인 / 해소된 Blocker

최종 `git fetch --no-progress origin main`은 exit0으로 성공했다. `origin/main`은 `76968ec8338c454a7a71bd2d641fb1bf2125098d`, 로컬 dev/gui HEAD는 시작과 같은 `3d70e4e8650dbfdd896bf0ddab0a5a57410ca304`다. `git rev-list --count --left-right origin/main...HEAD` 원문은 `35 1`. AGENTS.md §2는 N M 차이를 해결한 뒤 편집하도록 하고 공유 기본 체크아웃의 merge를 금지한다. 따라서 stage/commit/merge/push를 하지 않고 리더의 기준 정리를 기다린다. index는 비어 있었고 전체 status는379항목이었다. 이번 카드 밖의 변경은 보존했다. 소스·시험·보고서·원문은 이 작업 트리에 보존돼 있다. 이는 시험 실패나 훅 실패가 아니다. 정상 훅은 아직 실행하지 않았다.

## 최종 Claim

GUI-C-267의 폐기 비선형·다항 보정 앱 참조와 전용 시험을 제거하고 단일 pipeline 진입점 검증은 보존했다. 단일점 gain과 generic native alert/error 처리는 유지했다. 새 PRE에서 구형 다항 gain은 오류 -4와 XPE_ERR_XCAL_TYPE으로 거부되며 기존 GUI 상태·로그 경로로 표시됐다. 지정 grep 잔여8행은 아래 표대로 무관한 코드3행과 리더 문서 범위5행이다.

## 최종 Evidence / Baseline-attribution

- GUI 기준 `D:/workspace-github/xpe-gui`, dev/gui, HEAD `3d70e4e8650dbfdd896bf0ddab0a5a57410ca304` + 이번 diff. PRE 기준 `d3216b2a6253e9e562032acab7370e7be11bc15f`. 두 Native 실행의 소스434파일 해시가 같고, 완료 후 현재 파일과의 불일치0건이다(`complete-staging/source-integrity.json`).
- 빌드7개 exit0, Integration `Total tests: 1022 / Passed: 1019 / Skipped: 3`, failed0. 정확한 명령·경고·SKIP3개는 아래 실측 절과 원문에 있다. 이 단계는 Native 환경을 제거한 managed 검증이다.
- 완전한 스테이징 전체 Native E2E: 2026-11-17 00:16:04.2119378~00:57:39.1935524 KST, exit0. 명령: `dotnet test clients/ImageProcTest.E2ETests -c Debug --no-restore --logger "console;verbosity=detailed" --logger "trx;LogFileName=e2e.trx" --results-directory "D:\workspace-github\xpe-gui\.moai\reports\lane-gui\GUI-C-267\complete-staging"`. 필터 없음. 원문: `Test Run Successful. / Total tests: 369 / Passed: 345 / Skipped: 24 / Total time: 41.5628 Minutes`. TRX failed/error/timeout/aborted 모두0. 종료 appAfter 빈 배열.
- 첫 전체 실행은 자체 스테이징의 CSV·provenance 누락으로 `369=332P/12F/25S`였다. 원문을 보존하고 스테이징 자료만 추가했다. 이후 기존 실패12개 모두 PASS, native-alert 재초기화 시험1개는 SKIP→PASS. 소스·단언·대기·DLL 변경으로 실패를 숨기지 않았다. 최종 SKIP24개 이름·사유 원문은 `complete-staging/nonpassing.md`, 이전37개 비통과 원문은 `first-full-nonpassing.md`다. SKIP는 통과로 계산하지 않는다.
- 두 실행이 남긴 임시 SelfCheck 폴더는 각17파일을 ZIP에 보존하고 모든 entry SHA256 일치를 확인한 뒤 해당 폴더만 비재귀 정리했다. `first-full-A05-11300.zip`: `0A6202134D363212A650B77053DC2DE3C9012C218989B56853D6652CC3E7B5AC`; `complete-staging/A05-10576.zip`: `8B7CE713E5E5BFCCA188D835C6EB3779D136E1FE26441D4764E601F0D495569D`. 정리 JSON에 원래 절대경로·시각·부재 확인을 보존했다. 구형 보정 원본5파일은 전후 SHA256·mtime 동일하다.

## 최종 Gaps / Residual-risk

실제 장비·임상 데이터·전역 입력 시험은 수행하지 않았다. E2E SKIP24개와 Integration SKIP3개는 미검증으로 남는다. 빌드 경고의 신규/기존 귀속은 별도 baseline 대조하지 않았다. Native 스테이징은 PRE/common의 지정 로컬 빌드와 기존 다른 모듈 bundle을 혼합한 것이며, 독립 인증이나 모든 모듈의 d321 빌드를 주장하지 않는다. 제품 문서의 옛 단계 설명5행은 리더의 문서 갱신 범위다. 마지막 fetch는 성공했으나 원격/main 차이35/1이 있어 커밋은 보류했다. Reviewer·main 병합·push는 리더 판정으로 남긴다.

---

아래는 수신·조사·실행 경과다. 당시의 대기/미확인 문장은 그 시점 기록이며, 현재 판정은 위 최종 절과 확정 실행 증거를 따른다.

## 첫 전체 E2E 결과와 시험 준비 결함

23:31:48~00:13:17, exit1, `Total tests: 369 / Passed: 332 / Failed: 12 / Skipped: 25 / Total time: 41.4759 Minutes`. 삭제한 E2E Fact2개와 기존371→369 분모가 일치한다. 원문 `e2e.stdout.txt`, `e2e.stderr.txt`, `e2e.trx`, `e2e.result.json`은 덮어쓰지 않는다. 종료 뒤 appAfter는 빈 배열이다.

GUI 레인의 스테이징이 기존 bundle의 `vg_table_water_csi600_victre.csv`와 provenance를 빠뜨렸다. P03/C06/P09 원문은 해당 CSV의 절대경로 부재를 명시하며, NativeProvenance 시험은 provenance.json 부재를 명시했다. 다른 virtual-grid 픽셀·알림 실패까지 전부 같은 원인이라고 아직 확정하지 않는다. 기존 bundle의 CSV(23,102 bytes, SHA256 `0bd7790d9b36e7ada6306f16c9889ec381361ef14e1569279ead856308ae7ca4`)만 카드 native에 복사하고, 원래47파일 해시가 그대로임을 확인했다. 기존 bundle provenance는 `baseline-bundle-provenance.json`으로 보존했다. 새 native/provenance는 실제47파일의 md5/SHA256/크기/출처를 기록하고, PRE d3216b2a + 기존 bundle 혼합임을 명시한다. 하나의 CI 빌드나 독립 인증 결과로 위장하지 않았다(`staging-correction.json`).

앱 소스·단언·대기·환경 변수·DLL은 바꾸지 않고, 완전한 스테이징에서 필터 없는 전체 E2E를 별도 `complete-staging/`에 기록한다. 이전 실패를 삭제하거나 통과로 바꾸지 않는다.

## 23:26 확정 엔진 수신·실행

엔진 기준은 `d3216b2a6253e9e562032acab7370e7be11bc15f`다. 추가 삭제된 `xpe_calib_get_max_points`, `xpe_calib_get_poly_degree` 및 `xpe_calib_set_mode`의 앱 선언·호출은 검색 결과 없었다. 아래의 이전 대기 기록은 이 수신으로 해소됐다.

PRE `build/ci-preprocess/bin` 산출물을 카드의 `native/`에 별도 복사했다. `xpe_preprocess.dll` SHA256은 `B8187C75FEC99B099584EE36B938E3D1AB746522DB14A64035D52CA86C017563`, `xpe_common.dll`은 `07D20B6BF4DA938FF878980DCEA497931DD865E1700E9D32E366E34AA06B95A9`다. 원본/복사본 일치는 `native-manifest.json`에 기록했다. DLL 생성 시각은 커밋보다 앞선 23:14이며, PRE 모드 축소 빌드 로그와 현재 runtime 소스·헤더·도구55파일의 d3216b2a 대비 불일치0건으로 귀속을 대조했다. 다른 모듈은 기존 native bundle이므로 전체 모듈이 이 PRE 커밋에서 빌드됐다고 주장하지 않는다.

23:30:32~23:31:08에 기존 다항 gain 파일을 사용한 Native GUI 자동화는 exit0, timeout=false였다. `gain-rejection-automation.json`의 `PreprocessRan=false`, `PreprocessStages="Loading gain calibration failed (-4)."`와 보존한 runtime log의 `XPE_ERR_XCAL_TYPE: calibration file type 3 (gain polynomial, removed in QA-A-336) is not supported` 및 `PREPROCESS_NOT_RUN`을 확인했다. 이는 다항 보정 성공이 아니라 명시적 거부와 기존 오류 표시 확인이다. 입력 보정5파일의 전후 SHA256·mtime은 모두 같았다. runtime log 복사본 SHA256: `c17a956e9f80dbf583751e3a1a6dda80f196f6118082db70b3b939a8ecc88a32`.

전체 E2E는 23:31:48에 `dotnet test clients/ImageProcTest.E2ETests -c Debug --no-restore --logger "console;verbosity=detailed" --logger "trx;LogFileName=e2e.trx" --results-directory "D:\workspace-github\xpe-gui\.moai\reports\lane-gui\GUI-C-267"`로 시작했다. 필터·중도 실패 중단 옵션은 없다. Native3 변수는 카드 native 폴더에 고정했다. 시작 PID7984, 원문은 `e2e-launch.stdout.txt`, 기준 소스와 환경은 `preflight.json`에 보존했다. 아직 결과가 없어 통과·실패 수는 미확인이다. 실행 중인 시험은 중복 시작하지 않는다.

## 23:08 추가 지시 수신·대조

PRE 제거 커밋 `2d9576de`를 로컬 객체에서 읽었다. 그러나 리더는 **그다음 보정 모드 축소 커밋으로 빌드된 DLL**을 사용하라고 지정했으므로 2d9576de/기존 DLL로 E2E를 시작하지 않았다. 최종 읽기 확인의 dev/preprocess HEAD는 QA-A-335 병렬 기준선 문서 `7031598b`이며, 이는 요청된 모드 축소 구현 커밋이 아니다. PRE336 report도 보정 모드 축소 미수행을 명시한다.

2d9576de 헤더 diff에서 삭제 API는 `xpe_calib_generate_nonlin_lut`, `xpe_calib_load_nonlin_lut`, `xpe_calib_unload_nonlin_lut`, `xpe_calib_generate_gain_polynomial`, `xpe_nonlinearity_correct`다. 현재 앱에는 지운 nonlinearity import가 없고, 지정 nonlin/polynomial 검색의 코드 잔여는 무관한 PR-FUNC-006 세 행뿐이다. 별도 삭제 export 검색도 출력 없음(exit1)이었다.

추가 지시가 말한 `clients/Nonlinearity*Tests`는 현재 index 경로 검색에서 `Functional/NonlinearityLutCallSiteTests.cs`와 `P1AReady/NonlinearityStageWiringTests.cs` 두 파일이 확인됐고 둘 다 작업 사본에서 삭제됐다. `NonlinearityBypassedScenarios.cs`도 삭제돼 비선형 E2E 경로가 남지 않는다. 실제 발견 수를3개 Tests라고 바꾸어 보고하지 않는다. pipeline 단일 진입점 Fact1은 폐기 기능과 무관한 검증으로 이름을 바꿔 보존했다.

의존성: PRE QA-A-336 완료 커밋 SHA와 그 커밋으로 빌드한 DLL 폴더·빌드 로그가 아직 없다. 질문 채널로 전달을 요청했다. 마지막 확인에서 dev/preprocess는 QA-A-335 커밋 a72d59c9, QA-A-336은 evidence 폴더만 있고 report.md는 없었다. 이전 DLL로 E2E를 대체 실행하지 않는다. source 변경은 stage/commit하지 않고 보존한다.

## 앱 변경·시험 분모

- 다항 전용 PolynomialCalibrationApplicationFixture와 이를 사용하는 ClampAlertOnScreenScenarios / NonlinearityBypassedScenarios를 제거했다(E2E Fact2개, Theory없음).
- --gain-poly 생성/성공을 전제한 GainPolyClampAlertTests Fact3개, 폐기 API 직접 호출 NonlinearityStageWiring Fact2개, 폐기 LUT 경로 NonlinearityLutCallSite Fact2개를 제거했다(Integration Fact7개, Theory없음). PreprocessModuleLeakTests의 직접 호출·파일 목록도 정리했다.
- pipeline 단일 진입점 검증 Fact1개는 `P1AReady/PreprocessPipelineCallSiteTests.cs`로 이름을 바꿔 보존했다. 새 파일은 아직 untracked이며 최종 stage에 포함해야 한다.
- 비선형 카탈로그 정의/순서/의존규칙, disabled 라디오3개·AppSettings 속성·Mock 분기·보고/변경감지·기본 JSON, 사용하지 않는 P/Invoke와 managed bypassNonlinearity 설정/문구를 정리했다. native struct layout·단일점 gain·generic alert drain·기존 gain 적재 오류 표시는 보존했다.
- 관련 기대값만 조정: distinct binding41→40, unsupported radio12→9. 기존 parser 문구 시험은 같은 parser 계약을 중립 예시로 검증하도록 바꿨다. 다른 기능 단언·대기를 느슨하게 만들지 않았다.

## 지정 grep 잔여8행과 각 이유

`git grep -n -i -I -E 'nonlin|polynomial|gain-poly|FUNC-006|FUNC-027' -- clients/ gui/ ':(exclude)**/bin/**' ':(exclude)**/obj/**' ':(exclude)**/TestResults/**'` 원문은 references-before.txt / references-after.txt(135→8행). 추적·미추적 일반 텍스트451개를 같은 제외 조건으로 별도 스캔해도8행이었다. **0건으로 보고하지 않는다.**

| 파일:행 | 유지 이유 |
|---|---|
| clients/ImageProcTest/MainWindow.xaml.cs:1983 | PR-FUNC-006/007은 고급 fractional 처리 요구번호로 CALIB 비선형 기능과 무관한 패턴 일치. 다른 기능 변경 금지에 따라 유지. |
| clients/ImageProcTest/Services/AlgorithmValidationCatalogService.cs:37 | 고급 Fractional processing의 PR-FUNC-006. 같은 이유로 유지. |
| 위 파일:39 | 고급 ROI-aware EI refinement의 PR-FUNC-006. 같은 이유로 유지. |
| gui/ImageProcTest/README.md:68 | 비선형 기본값을 설명하는 기존 문서. 문서 수정은 리더 범위이므로 보존하며 코드 제거 후 문구 정합성 갱신 필요. |
| 위 README.md:152 | 옛 pipeline 단계 설명. 같은 문서 소유 범위이며 최신 동작으로 검증된 문구라고 주장하지 않는다. |
| gui/ImageProcTest/help/quick-start.html:151 | 옛 nonlinearity bypass 설명. 이번 카드에서는 문서를 수정하지 않았으며 리더 갱신 대상으로 남김. |
| gui/ImageProcTest/help/scope.html:22 | 옛 전처리 단계 나열. 같은 문서 잔여. |
| 위 scope.html:50 | 옛 pipeline/stage fallback 설명. 같은 문서 잔여이며 최신 계약임을 보증하지 않음. |

## 현재 실측·증거

시작 HEAD3d70e4e8 + 위 앱/시험 diff, `D:/workspace-github/xpe-gui`에서 실행. 22:34:49→22:35:08에7개 프로젝트를 `dotnet build <project> -c Debug --no-restore --nologo`로 빌드했다. legacy GUI / 현대 GUI / SelfCheck / 내부E2E / Integration / E2E / GuiObservation 모두 exit0·errors0. 경고 수는 각각0/22/1/0/13/7/0이며 원문 보존, 기존 경고인지 새로운 경고인지 별도 baseline 대조는 하지 않았다.

Integration 명령은 `dotnet test clients/ImageProcTest.IntegrationTests -c Debug --no-build --no-restore --logger "console;verbosity=normal" --logger "trx;LogFileName=integration.trx" --results-directory "D:\workspace-github\xpe-gui\.moai\reports\lane-gui\GUI-C-267"`. 22:35:08.3691722→22:35:34.7511175 KST, exit0, `Test Run Successful. / Total tests: 1022 / Passed: 1019 / Skipped: 3 / Total time: 25.1991 Seconds`. TRX executed1019/failed0/aborted0/timeout0, NotExecuted3. 이전 GUI-C-266 실측 total1029에서 폐기 Fact7개 제거와 분모 감소가 일치한다. E2E의 전체 분모는 아직 측정하지 않았다.

자식 환경에서 Native3와 XPE_GUI_DATA_DIR를 제거했다. 이는 DLL 의존 없는 managed 단계 검증이며 새 PRE 엔진 통합 검증이 아니다. 원문은 build-*.stdout.txt / stderr.txt / result.json 및 integration.stdout.txt / stderr.txt / trx / result.json, 도우미 run-managed-checks.ps1이다.

SKIP:
1. `Safety.DllSearchPathSafetyTests.AFileLinkInsideAnApprovedFolder_ThatPointsOutside_IsRejected` — `a file symbolic link could not be created here (needs developer mode or elevation): 클라이언트가 필요한 권한을 가지고 있지 않습니다. : 'C:\Users\drake\AppData\Local\Temp\xpe_c225b_symlink_9987a922fb6449cf9e1eea313bebc6c5\approved\xpe_common.dll'`.
2. `Functional.BaselineFingerprintProbe.PrintTheStageHashesOfTheBaselineChainOnRealData` — `XPE_NATIVE_DIR, XPE_C231_CALIB_DIR and XPE_C231_RAW are needed.`
3. `Functional.OffsetQualityTests.TheRealModule_OnDark16AndTheCleanSetMap_MeasuresTheSignedBiasThePaperReports` — `XPE_C253_NATIVE_DIR/XPE_C253_DARK/XPE_C253_OFFSET not set - the dark frame and the map are not in the repository. XPE-SKIP-ALLOWED:273`.

## 남은 필수 작업 / Gaps

PRE336 커밋·빌드 DLL 근거와 최종 헤더를 확인해야 한다. 그 DLL에서 구형 다항 gain 거부→기존 앱 오류 표시를 관측하고, 지정 새 DLL로 필터·StopOnFail 없는 전체 E2E1회를 수행해야 한다. 그 결과와 남은 문서 참조를 포함해 최종 dev/gui 커밋한다. 현재 카드 완료·새 엔진 통합 성공·전체 E2E 성공을 주장하지 않는다. 타 작업/공유 데이터/설정·제품 문서 수정·push 없음.

시작 HEAD `3d70e4e8650dbfdd896bf0ddab0a5a57410ca304`, dev/gui. PRE dev/preprocess의 현재 최신 기록은 QA-A-334 문서 커밋 ea8d1f41이며 QA-A-336 커밋·DLL을 아직 확인하지 못했다. E2E는 지정 PRE 커밋으로 빌드된 DLL 근거가 확인된 뒤에만 실행한다. 우선 앱 참조와 기존 보정파일 오류 경로를 읽기 조사한다. 제품 문서(SRS)·다른 기능·공유 설정 수정, push 없음.

## 진행·의존성

- baseline 참조 원문 `references-before.txt`: 지정 패턴 대소문자무시135행, clients/gui에서 bin/obj/TestResults 제외. 다항 전용 fixture·양성 시험·#194 전용 기대문구와 disabled Nonlinearity 옵션의 직접 연결 코드만 제거한다. 문서·무관한 고급모듈 동명 요구번호는 유지하고 잔여행별 이유를 기록한다.
- 구현 담당에게 코드·관련 시험 정리만 배정, 루트는 PRE DLL 근거·빌드/시험·보고/커밋 담당이다. 기존 단일점 gain·generic alert drain·오류 표시 경로를 보존한다. PRE 새 헤더가 확인되기 전 struct layout은 임의로 바꾸지 않는다.
- remote default는 origin/main으로 확인. `git fetch --no-progress origin main`은 exit128, 원문 `fatal: unable to access 'https://github.com/holee9/image-processing.git/': getaddrinfo() thread failed to start`. 원격 최신 상태를 확인했다고 주장하지 않는다. 현재 로컬 PRE336 완료 커밋·보고서·DLL은 아직 미확인이다.

## 기존 오류 경로·구형 파일 읽기 근거

탐색 담당이 기존 구형 파일 `C:/Users/drake/AppData/Local/Temp/xpe-c129-e2e-3ba974a89b1f488c9d3c5ca8120cf475/gain_poly.xcal`을 확인했다. width/height1024, 12,583,382 bytes, SHA256 `2b313710eb5c296377ce97335499888b6864341c64a7d7d15ec3d4ef80dae4ec`. 같은 폴더 gain.xcal도 동일 SHA이며 offset.xcal·defect.xcal·manifest.json이 있다. 원본을 수정·삭제하지 않았다. manifest의 gain 해시는 기존 scalar값이어서 현재 gain과 다르며, 앱 fixture가 파일3개 존재만 확인하는 경로와 manifest를 검증하는 도구 경로를 구분해야 한다.

현 코드 경로: `GuiPreprocessRunner.cs:85–96` gain load 비0 → `Loading gain calibration failed ({code}).` → `RealXpeBackend.cs:394–412` → `BaselineStageAdapters.cs:36–37` → `ProcessingChainRunner.cs:69–71` RequestedNotApplied/입력 유지 → `MainWindowViewModel.cs:3335–3346,3352,3382–3389` 상태 reason·로그·WARN PREPROCESS_NOT_RUN. RawSizeRules는 맵 크기를 먼저 검사한다. 이 자료는 읽기 추적이며 **새 PRE DLL의 실제 거부·화면 표시를 검증한 것은 아니다**. 최종 편집 후 행번호는 다시 대조한다.
