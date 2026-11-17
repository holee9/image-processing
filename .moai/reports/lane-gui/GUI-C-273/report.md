구현·실제 쌍 재캡처 완료 / 전체 E2E 0실패 요건 미충족 — GUI-C-273, 리더 검토 대기. Refs #285.

## Claim
O: 큰 이상값 하나 때문에 작은 차이가 흰색으로 사라지는 문제를 보완한다.
KR: 기본 ±(5·s), 범위 밖 포화, 범례의 실제 범위·포화 수, 극값 선택, 합성 쌍 검증, 실제 s5/001 재캡처. 이 기능들은 구현·관측했으며 전체 E2E 0실패는 달성하지 못했다.

| 변경 파일·함수 | 전후 동작 |
|---|---|
| gui/ImageProcTest/Views/ComparisonImportWindow.cs:53, 생성자 | 극값 선택 체크박스 추가. 초기 상태는 기존 표식 척도와 적용 k(기본5)를 사용한다. |
| 같은 파일:484 UpdateMarkersAsync / :595 RefreshDifference | 적용된 표식 k와 색 범위를 함께 갱신한다. 표식 계산 서비스·기준은 수정하지 않았다. |
| 같은 파일:601 DifferenceBitmap | 기존 전체 극값 기준 → 기본 ±k·s, 범위 밖 끝 색으로 포화. 범례에 범위·포화/유한 표시 화소 수를 표시하며 극값도 선택 가능하다. |
| clients/ImageProcTest.E2ETests/Rendering/ImportedComparisonTests.cs:59,86,120 부근 | +3/큰 이상값, 양의MAD, 0, 비유한·overflow 범례, 극값 전환·복귀를 검증한다. 기존 극값 픽셀 시험은 극값 선택을 명시한다. |

s=0이면 0만 흰색이고 비영 차이는 끝 색이다. 비유한 차이는 자홍색이며 포화 분모에서 제외한다. k·s가 유한 범위를 넘으면 극값으로 전환하고 범례에 `extrema fallback: k·s is non-finite`를 명시한다.

## Evidence
모든 명령은 전용 작업 트리에서 env 래퍼를 통해 실행했다. dotnet 경로는 C:/Progra~1/dotnet/dotnet.exe, 래퍼는 D:/workspace-github/xpe-gui/.moai/reports/lane-gui/GUI-C-254c/env.cmd이다. 로그 원문은 이 보고서와 같은 폴더에 보존했다.

최초 RED 명령:
`dotnet test clients/ImageProcTest.E2ETests -c Debug --filter FullyQualifiedName~C273 --logger "console;verbosity=normal"`
원문 red.txt: `Expected: [0, 0, 255, 255]`, `Actual: [255, 255, 255, 255]`, 총1/실패1. +3/65532가 같은 흰색으로 반올림되는 기존 원인을 재현했다.

overflow 범례 RED:
`dotnet test clients/ImageProcTest.E2ETests -c Debug --no-restore --filter FullyQualifiedName~C273_PositiveMad --logger "console;verbosity=normal"`
red-overflow-label.txt: 총1/실패1. 범례 설명 부재를 재현한 뒤 보완했다.

최종 영향 시험:
`dotnet test clients/ImageProcTest.E2ETests -c Debug --no-restore --filter "FullyQualifiedName~C273|FullyQualifiedName~C270_OnePositionToggle|FullyQualifiedName~C270_ZeroDifference|FullyQualifiedName~ComparisonMarkerTests" --logger "console;verbosity=normal"`
green-final-boundaries.txt 원문: `총 테스트 수: 12`, `통과: 12`. 양의 s=1.4826 → 범위7.413, +3 BGRA [152,152,255,255], 포화1/511. 극값 선택 시65532, 포화0/511, 재전환 동일. s=0의 비영 포화와 전부0의 흰색·포화0도 단언했다.

빌드:
`dotnet build gui/ImageProcTest/ImageProcTest.csproj -c Debug --no-restore --verbosity minimal`
build-app-final.txt: 오류0/경고22. 경고 증감의 부모 기준은 측정하지 않았다.

전체 명령은 `dotnet test clients/ImageProcTest.E2ETests -c Debug --no-restore --logger "console;verbosity=normal"`이다.

| 실제 실행 조건·원문 | 결과 |
|---|---|
| 첫 실행 run-full-mock.cmd / green-full-mock.txt: Mock, 존재하는 빈 NativeDir, exclusive1, 별도 데이터 | 370 = 271P / 3F / 96S, exit1 |
| 최종 run-final-mock.cmd / full-final-mock-native-unset.txt: Mock, NativeDir 미설정, exclusive1, 새 데이터, 전역 입력 미허용 | 370 = 269P / 2F / 99S, exit1 |
| run-ab-parent.cmd / ab-parent.txt: 부모 코드의 같은 두 UIA 시험, 최종과 같은 환경 | 2P / 0F |
| run-ab-candidate.cmd / ab-candidate.txt: 수정 코드의 같은 두 UIA 시험, 같은 환경 | 2P / 0F |

첫 실행에서 존재하는 빈 폴더가 AutomationReportBackendTests.cs:864-870 NativeDirectoryOrSkip 조건을 통과시켜 A20/A03 Native 전용 시험을 실행시켰다. .github/workflows/ci.yml:1040-1048 Mock 조건에는 NativeDir 설정이 없다. 최종 실행에서 두 시험은 Native 자료 미제공 사유로 SKIP하는 것을 실제 확인했다. 첫 실행을 삭제하거나 성공으로 바꾸지 않았다.
최종 실패는 C270_ImportedOutputs_DisplayBothAndToggleWithoutComparabilityBlock의 ComparisonImageA 검색과 C271_RegionsAreListed_AndSelectingOneZoomsAndCentersBothViews의 ComparisonMarkerList 검색 null이다. C271 ImportedMarkerTests.cs:53 managed 목록3개 단언은 통과한 뒤 :64 UIA 검색에서 실패했다. 앞선 부모 영향 묶음에서도 C270 실패를 관측했고, 마지막 두 시험 단일 A/B는 모두 통과했다. 전체 실패의 원인 및 이번 변경과의 인과는 미확정이다. 시험 동기화 코드는 고치지 않았다.

실제 쌍 D:/workspace-github/xpe-2g-offline/compare-s5_csi_post-001.e2e.json을 최종 앱에서 열었다. 열기→표시·자동분석 1523.9021ms. 실제 범례 원문:
`Signed original-value A−B (scale k·s): blue −2.10347 · white 0 · red +2.10347; saturated 588094/1966080 display pixels (포화 화소 수).`
차이 영상이 색으로 표시되는 것을 실제 창 캡처로 확인했다. 후보1377/표시20, s=0.4206942, k5, N16, step2, 결함 맵 엇갈림0 및 넓은 차이 경고가 유지됐다. MODE_MISSING/MAP_IDENTITY_DIFFERS, VENDOR_LIVE_BLOCKED와 unreviewed contract 안내도 유지됐다. 비교 적합성·영상 품질 통과 판정은 아니다.

[로컬 최종 캡처](D:/workspace-github/xpe-gui/.moai/reports/lane-gui/GUI-C-273/s5-csi-post-001-robust-final.png): 1166×1193, 1345297B, SHA256 44b694a06e922019845ce583465216450930f2b2a0c554abc130cdad00838d9d. 실제 JPEG 캡처를 PNG로 인코딩했고 디코딩 RGB 동일성을 확인했다. 원본·첫 캡처도 보존했다. 영상 커밋 금지에 따라 모든 캡처는 로컬에만 남긴다.
검증 앱 PID29904 정상 종료(closed=true,killed=false). 도우미의 첫 modal 종료 후 stale-element 오류는 원문을 보존하고 새 attach로 main 정상 종료를 확인했다. Sky 읽기 조회에서 이 작업 트리의 잔류 창은0이었다. 전체 프로세스 목록이 없다는 주장으로 확대하지 않는다. 실제 전역 입력 및 XPE 처리 엔진 실행은 하지 않았다.

## Baseline-attribution
트리 C:/Users/drake/.moai/worktrees/projects/image-processing/GUI-C-273, 브랜치 WT-gui-difference-scale. Moai 생성 시 f15b01abe8510cfc43dd868d4e5648bb75c75291였고 origin/HEAD=origin/main을 확인했다. f15와 원격 main ff4bf614의 git diff --quiet exit0 이후 전용 트리만 merge --ff-only origin/main으로 fast-forward했다. primary 브랜치 상태는 변경하지 않았다.
최종 기준 HEAD ff4bf6143cd2d477e77a13a00801b29432e7e840 + 이 카드의 미커밋 코드로 빌드·최종 전체 시험·최종 캡처를 수행했다.
최종 ImageProcTest.dll SHA256 642fd8dd3ed22d9ed33e849611f19ece54e60ec1555a422ea1a6e8ddd9cc8d5c. 캡처 이후 최종 전체 시험 중에도 같은 값이었다. apphost EXE SHA256 57fca5891e5431441710035a49e9480b1d08dde17bf91710a6d0c80c0e05a329, ProductVersion1.0.0+ff4bf6143c. EXE 해시만으로 수정 어셈블리를 식별하지 않는다.
UIA 실제 창 조작은 기존 GUI-C-272 driver-v3의 PID/EXE 소유 검증과 UIA 패턴을 재사용했다. Sky는 창 조회·캡처에만 사용했다. Python 계약 검증은 기존 비교 창의 실제 child이며 처리 엔진이 아니다. 캡처·실행 바이너리 해시는 capture-final-manifest.json에 보존했다.

## Gaps
전체 E2E 0실패는 미충족이다. 리더/Reviewer의 UIA 실패 판정과 후속 검증이 필요하다. 단일 A/B 통과만으로 회귀가 없다고 주장하지 않는다. Native 및 전역 입력 시험은 이번 제한으로 미실행이며 99 SKIP에 포함된다. 별도 커버리지·LSP 진단·영상 품질·픽셀별 실제 보정 적용 감사는 미측정이다. 180쌍 전체를 GUI로 재확인하지 않았다.

## Residual-risk
강건 범위는 작은 차이를 드러내는 표시 선택이며 통계·원본 픽셀·표식 판단을 바꾸지 않는다. 포화 수의 분모는 표시 샘플 중 유한 차이이고 원본 전체 화소가 아니다. s=0에서 비영 차이는 모두 포화하므로 크기 구별은 극값 선택으로 확인해야 한다. 미확정 UIA 실패를 이 카드에서 임의 수정하지 않았다. 범례·색상 구현은 검토 가능하지만 카드 전체 종료 판정은 리더에게 요청한다.
