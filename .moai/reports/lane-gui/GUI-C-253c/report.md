[2026-10-17 23:04 KST]

# GUI-C-253c 보고서 — 기준 행 출처 표기와 입력 종류 변경 시 측정 행 무효화 (Codex #241 R241-2, R241-3)

기준 커밋: 01da4c4c (GUI-C-253b). 관련 이슈: #273.

## 1. 한 일

### R241-2 — 기준 행이 실제 상태를 말한다
- 측정이 이뤄진 경우: `입력 종류: 다크 프레임(사용자 선택, 취득 메타데이터로 확인하지 않음)`. 조건 문구는 `측정 조건: …` 라벨을 붙여 "조건이 충족됐다는 뜻이 아님"을 구분했다.
- 미확인·다크 아님·offset 맵 없음인 경우: `미측정 — <사유>` 와 상태 `N/A`. 이때 `측정 조건:` 은 측정이 요구하는 조건이며 충족하지 않으면 측정하지 않는다고 적었다. 더 이상 "다크 프레임으로 확인된" 이라고 말하지 않는다.

### R241-3 — 입력 종류를 바꾸면 새로 추가한 측정 행만 즉시 무효화
- `OffsetQualityRows.Invalidate(rows, newKind)` (순수 함수): 측정 행(기준 행 + 신규 8행 + 도움말)만 골라 바꾸고 기존 네 행은 값·판정·순서 그대로 둔다. 측정 행이 없으면 같은 목록 객체를 돌려준다.
- 다크 선택: 사유 "입력 종류 변경 — 다시 실행 필요". 다크가 아닌 종류: 그 종류의 사유. 다시 "다크 프레임"을 골라도 옛 숫자는 돌아오지 않는다(무효화는 입력 종류만 보고, 숫자를 저장해 두지 않는다).
- `MainWindow.RawInputKindComboBox_SelectionChanged`: `lastNativePreviewResult` 와 `DarkMetricsGrid` 만 갱신한다. 미리보기 영상·기존 네 행·판정은 그대로, 네이티브 호출은 하지 않는다.
- `DarkMetricsGrid` 에 `VirtualizingPanel.IsVirtualizing="False"` 를 주었다. 화면 밖 행이 UI Automation 에 보이지 않아 시험이 일부 행만 읽었기 때문이다(행 수는 13개 안팎).

## 2. 증거

| 확인 | 명령 | 결과 |
|---|---|---|
| 순수 시험(R241-2, R241-3) | `dotnet test clients/ImageProcTest.IntegrationTests --filter OffsetQualityTests` (`run_offset_quality_tests.txt`) | 통과 29 / 실패 0 |
| 실제 창 시험 K1 | `dotnet test clients/ImageProcTest.E2ETests --filter LegacyInputKindScenarios` (`run_k1_uia_restored.txt`, 사본 `run_k1_uia.txt`) | 통과 1 |
| 반증 | 핸들러의 `DarkMetricsGrid.ItemsSource = rows;` 한 줄을 비활성화 (`run_k1_falsified_grid_not_updated.txt`) | K1 실패(Assert.Contains: 무효화 행 없음). 원복 후 재빌드, 위 통과 재확인 |

K1 시험 흐름(UI Automation, 전역 키보드·마우스 입력 없음; 대화상자는 값 패턴 + 해당 창으로의 BM_CLICK):
1. 1024x1024 원시 영상과 `calibration/cdark.raw` 를 열고 Offset 단계를 켠 뒤 "Run Selected". 입력 종류는 기본 `미확인` → 측정 행 모두 `N/A`, 기준 행 `미측정 — 다크 프레임 미확인 …`, 기존 4행 존재.
2. 실행하지 않고 `다크 프레임` 선택 → 측정 행이 `입력 종류 변경 — 다시 실행 필요`. 기존 4행 동일, 산출물 폴더 수 불변(미리보기가 돌지 않음), 미리보기 문구 불변.
3. "Run Selected" 다시 → 서비스가 `다크 프레임` 을 받아 측정: 기준 행 `입력 종류: 다크 프레임(사용자 선택, …)`, 측정 행 7개 이상이 `REPORTED`. 산출물 폴더 +1.
4. `임상·기타 영상` 선택 → 측정 값이 모두 사라지고 그 종류의 사유. 다시 `다크 프레임` → 옛 숫자 없음, "다시 실행 필요".
5. 다른 파일을 열면 입력 종류가 `미확인` 으로 돌아간다.

관측 값(3번 단계, 이 합성 영상): DarkBias 부호 있음 901 화소값, DSNU 부호 있음 3.266 화소값, DarkReduction −2.0412 dB, ClampRate 0 %. 합성 영상이라 수치 자체는 의미를 주장하지 않는다. 이 시험이 보이는 것은 연결이다.

실행 환경: 사전·사후 남은 프로세스(ImageProcTest/testhost/xpe_ai_worker) 0, 여유 메모리 15566 → 15638 MB. 한 번에 시험 1개만 실행.

## 3. 미검증 / 한계

- 사용한 `xpe_preprocess.dll` 은 CI 산출물이 아니라 로컬 빌드(sha256 `d86042811c75e222…`, GUI-C-253 과 동일)다. 리더가 알려 주는 CI 산출물로 같은 K1 을 다시 돌리지 않았다.
- `NativeModuleSignatureParityTests.EveryDelegateBoundToAnExport_SaysWhatThatExportsHeaderSays` 는 dev/gui 단독에서 실패한다(헤더에 `xpe_verify_offset_signed_file` 선언이 없음). 이전 보고와 같은 상태이며 026ea384 헤더가 있는 통합 브랜치에서는 통과한다. 허용 목록으로 가리지 않았다.
- 전체 스위트는 돌리지 않았다(CI 몫). 입력 종류 선택 결과가 DICOM·다른 경로로 이어지는지는 범위 밖.
- K1 은 `XPE_NATIVE_DIR` 과 새 export 가 없으면 사유와 함께 건너뛴다(`XPE-SKIP-ALLOWED:273`).

## 4. 잔여 위험

- 무효화는 "마지막 실행 결과가 화면에 있는 동안"만 동작한다. 실행 결과가 없으면 아무것도 바꾸지 않는다(의도).
- 그리드 가상화를 끈 것은 행이 13개 안팎인 이 표에서만 안전한 선택이다. 행이 크게 늘면 다시 봐야 한다.

## 5. 변경 파일

- `clients/ImageProcTest/Services/OffsetQualityRows.cs` (기준 행 문구, `Invalidate`, `IsMeasuredRow`)
- `clients/ImageProcTest/MainWindow.xaml`, `MainWindow.xaml.cs` (콤보 `SelectionChanged`, 그리드 가상화 끔)
- `clients/ImageProcTest.IntegrationTests/Functional/OffsetQualityTests.cs` (순수 시험 11건 추가)
- `clients/ImageProcTest.E2ETests/ImageProcTest.E2ETests.csproj` (OffsetQuality* 링크)
- `clients/ImageProcTest.E2ETests/Scenarios/Legacy/LegacyPreprocessReadinessScenarios.cs` (UIA 도우미 4개, `FindExecutable` 접근 수준)
- `clients/ImageProcTest.E2ETests/Scenarios/Legacy/LegacyInputKindScenarios.cs` (신규, K1)
