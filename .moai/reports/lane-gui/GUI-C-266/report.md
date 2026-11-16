[2026-11-16 22:08 KST] 검증 완료 — 온도 카탈로그2행·preset키2곳 제거, 지정grep0건, 빌드2개 성공·Integration1026P/0F/3SKIP

지정 패턴의 추적 소스 baseline은 validation 카탈로그와 chain 카탈로그의2행이다. chain preset의 temperature 단계키도 함께 제거한다. baseline 정의 수는 validation33 / chain29. 두 서비스 참조와 시험의 count/temperature 사용을 검색했으나 직접적인 개수 기대 시험은 찾지 못했다. 기존 프레임워크·다른 카드 로컬 변경은 보존한다. 제품 문서(SRS 등)·다른 기능 수정, push 없음.

## Claim / diff

- `clients/ImageProcTest/Services/AlgorithmValidationCatalogService.cs`에서 SWU-1.6 온도 보정 정의1행을 삭제했다. 정의 수33→32.
- `clients/ImageProcTest/Services/AlgorithmChainCatalogService.cs`에서 temperature 단계 정의1행과 ProductCanonicalOrder / PreE2eProofOrder의 같은 단계키를 삭제했다. 정의 수29→28. 나머지 순서·실행 조건은 바꾸지 않았다.
- 두 서비스 직접 참조와 Integration/E2E의 관련 이름·temperature·SWU 사용을 검색했으나 이 목록 개수를 고정한 기대값은 발견하지 못해 시험 코드는 변경하지 않았다. 사용자 지시에 따른 온도 보정 항목 제거만 수행했다.

## Evidence / baseline-attribution

측정 트리 `D:/workspace-github/xpe-gui`, 시작 HEAD `1404272b712c150c5532c031fb93e0f277b3686c` + 위2개 파일 diff. 다른 카드·프레임워크 변경은 제외하고 보존했다.

검색 원문은 `grep.txt`. 실제 실행한 argv에 해당하는 명령:

```text
git grep -n -I -E 'TempCompensat|detectorTempC|xpe_temp_compensate|FUNC-008' -- clients/ gui/ ':(exclude)**/bin/**' ':(exclude)**/obj/**' ':(exclude)**/TestResults/**'
exit=1
stdout=""
matches=0
```

추적 여부에 관계없이 clients/gui의 일반 텍스트 파일을 별도 스캔했으며 bin/obj/TestResults와 NUL 포함 바이너리를 제외한456개 파일에서도 같은 패턴0건이었다. rg가 PATH에 없어 git grep을 사용했다. SRS 문서는 검색·수정 대상에 포함하지 않았다.

| 명령 | 관측 결과 | 원문 |
|---|---|---|
| dotnet build clients/ImageProcTest/ImageProcTest.csproj -c Debug --no-restore --nologo | exit0, Build succeeded, 0 warnings/0 errors | build-gui.stdout.txt / stderr.txt / result.json |
| dotnet build clients/ImageProcTest.IntegrationTests/ImageProcTest.IntegrationTests.csproj -c Debug --no-restore --nologo | exit0, Build succeeded, 14 warnings/0 errors | build-tests.stdout.txt / stderr.txt / result.json |
| dotnet test clients/ImageProcTest.IntegrationTests -c Debug --no-build --no-restore --logger "console;verbosity=normal" --logger "trx;LogFileName=integration.trx" --results-directory <현재 카드 절대경로> | exit0, Test Run Successful, total1029 / passed1026 / failed0 / skipped3 | integration.stdout.txt / stderr.txt / trx / result.json |

시험은 `2026-11-16T22:07:00.9523104+09:00`→`22:07:32.4182590+09:00`, 총30.4424초. TRX total1029/executed1026/passed1026/failed0/aborted0/timeout0, NotExecuted3개. 자식 프로세스에서 XPE_NATIVE_DIR / XPE_NATIVE_DIR_EXCLUSIVE / XPE_C231_NATIVE_DIR / XPE_GUI_DATA_DIR를 제거한 조건으로 실행했고 전역 환경은 바꾸지 않았다. 실제 명령·시각·HEAD는 result.json에 있다. 별도 앱·Native 엔진 실행은 하지 않았다.

SKIP 원문:

1. `Safety.DllSearchPathSafetyTests.AFileLinkInsideAnApprovedFolder_ThatPointsOutside_IsRejected` — `a file symbolic link could not be created here (needs developer mode or elevation): 클라이언트가 필요한 권한을 가지고 있지 않습니다. : 'C:\Users\drake\AppData\Local\Temp\xpe_c225b_symlink_68191834dede444cad7069ce15cc676d\approved\xpe_common.dll'`.
2. `Functional.BaselineFingerprintProbe.PrintTheStageHashesOfTheBaselineChainOnRealData` — `XPE_NATIVE_DIR, XPE_C231_CALIB_DIR and XPE_C231_RAW are needed.`
3. `Functional.OffsetQualityTests.TheRealModule_OnDark16AndTheCleanSetMap_MeasuresTheSignedBiasThePaperReports` — `XPE_C253_NATIVE_DIR/XPE_C253_DARK/XPE_C253_OFFSET not set - the dark frame and the map are not in the repository. XPE-SKIP-ALLOWED:273`.

## Gaps / residual-risk

시험 빌드에 경고14건이 있으며 원문을 보존했다. 이 카드에서 경고 baseline을 새로 측정해 비교하지 않았으므로 모두 기존 경고라고 단정하지 않는다. SKIP3개는 미검증이다. 카탈로그 직접 단위시험·실제 GUI 화면 동작·전체 E2E·PRE 레인의 엔진/API 제거 후 통합은 이번에 실행하지 않았다. 제품 문서 갱신과 PRE 병합은 리더 범위다. 새 환경 설치·설정 변경·시험 기대값 완화는 하지 않았다. 정상 staged whitespace 검사 후 dev/gui에 카드 변경만 커밋하고 push하지 않는다.
