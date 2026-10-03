# GUI-C-212 — 합성 오라클을 현재 API 로 고치고 시험으로 지킴 + 세 탭이 같은 준비도를 보임 (#249)

증거(이 폴더): `falsification_arms.txt`(8팔), `e2e_legacy_scenarios_output.txt`, `after_tabs_uia.txt` · `after_run_selected_uia.txt`(고친 뒤 같은 UIA 프로브 재실행), `ci_legacy_app_build.patch.txt`(워크플로 패치 초안). 앱 코드 변경이다 — Codex 대상.

## 바꾼 것

1. **오라클(`XpePreprocessSyntheticOracle.cs`)을 현재 계약으로 다시 썼다.** 보정 함수를 `(input, output, metadata)` 로 부르고, 교정을 **적재**한다: offset·gain 맵은 모듈의 `xpe_calib_generate_offset/gain` 으로 임시 폴더에 만들어 적재하고, defect 맵은 시험이 210 에서 쓴 방식 그대로(152바이트 XCal 헤더 + SHA-256) 앱 런타임에서 **임시 파일로 쓰고 끝나면 임시 폴더째 지운다**(`Path.GetTempPath()` 아래 `xpe_oracle_<guid>`). 판정은 단계마다 OK 이고 **효과가 있어야** 통과다(offset 은 입력을 바꿔야, defect 는 표시 화소만 고치고 나머지는 비트 동일해야, 전체 출력은 0 이 아니어야) — "NaN/Inf 0·RMSE 0" 이 아무것도 안 쓴 출력의 0 으로 통과하던 길을 닫았다. 세션 ID 는 빈 값. 같은 DLL 로 한 번 돌린 결과: `Passed=True`, 세 단계 OK(effect 100 / 0 / 58915), 출력 범위 900..1155, 입력 보존, RMSE 0.
2. **결과 레코드를 별도 파일로**(`PreprocessSyntheticOracleResult.cs`): 시험이 오라클을 링크하려면 필요했다(`NativeReadinessProbe.cs` 안에 있어 다른 프로브를 전부 끌고 왔다). 내용은 그대로.
3. **오라클을 부르는 시험(`PreprocessSyntheticOracleCallTests`, IntegrationTests)** — 3개: 스테이징된 DLL 로 오라클을 실제로 불러 `Passed`·세 단계 OK·offset/defect 가 입력을 바꿈·입력 SHA 보존·NaN/Inf 0·결정성 0·출력 범위가 있음을 단언 / 임시 폴더를 남기지 않음(폴더 수 세기에 대조군 포함) / DLL 없음이면 `Not run`·절대 통과 아님. `PInvokeWrapper.cs`·`NativeDependencyLoader.cs` 와 오라클·레코드가 IntegrationTests 에 링크된다(오라클은 델리게이트로만 DLL 을 불러 `XpeCommonApi` 정적 생성자 — 두 번째 DllImport 리졸버 등록 — 가 이 어셈블리에서 실행되지 않는다).
4. **평가 탭 = 준비도 판정 하나.** `ModuleReadinessGrading.IsProcessingEnabled(snapshots, 모듈)` 를 만들어 `MainWindow.IsNativePreviewReady()`(전처리, 예전엔 `IsExportReady` 만 봄)와 `IsEnhanceBasicPreviewReady()` 가 같은 함수를 부른다. 진단·교정 탭이 쓰는 `ProcessingEnabled` 와 같은 값이다. 단위 시험 3개(`ModuleReadinessProcessingEnabledTests`: 같은 R2 라도 꺼짐/켜짐 구별, 이웃 모듈이 켜져도 다른 모듈은 안 켜짐·없는 모듈은 꺼짐, 이름 대소문자).
5. **세 탭이 같은 상태를 보이는지 UIA E2E(`LegacyPreprocessReadinessScenarios`, clients/ImageProcTest.E2ETests).** 레거시 앱을 띄워 탭 선택(`SelectionItem.Select`)과 이름·활성 읽기만 했다(키보드·마우스 없음). R01: 오라클 통과 상태 — 진단 탭 `pass=True`+표 R3+꺼진 목록에 없음 / 교정 탭 NATIVE-NOT-READY 0건 / 평가 탭 `Preprocess=ready`+Offset 스위치 활성, **셋 다 참**. R02: 오라클이 못 도는 상태 — **환경 결함으로 만든다**(앱의 TEMP 를 파일 경로로 지정해 오라클이 임시 폴더를 못 만듦; 코드를 고치지 않음; 내보내기 점검은 여전히 통과라 옛 코드에서 탭이 갈라지던 바로 그 상태) — 진단 `Synthetic oracle exception; pass=False`·R2·꺼짐 / 교정 Hard 3건 / 평가 `Native pre/post exports are not ready…`·스위치 비활성, **셋 다 거짓**. 둘 다 통과(출력 증거 파일). `XPE_NATIVE_DIR` 가 없으면 두 시나리오가 사유와 함께 건너뛴다(Mock E2E 잡 상태). 바이너리 신선도 가드: `ImageProcTest.dll` 이 소스보다 오래되면 건너뛰지 않고 실패(.exe 는 런처 스텁이라 재빌드해도 안 바뀐다 — 처음 `.exe` 시간을 봤다가 고쳤다).
6. **`NativePreviewMetrics.InputPreserved` — 이름과 식이 맞지 않았다, 고쳤다.** 식 `changed == 0 && nanInf == 0` 의 `changed` 는 `|출력 − 원본| > 0.5` 인 화소 수라서 이 값은 "**출력이 입력과 같다**"다. 평가 프로토콜이 `InputPreserved` 를 `sha256(raw_before) == sha256(raw_after)` 로 정의한다(`Algorithm-Evaluation-Protocol.md:73`). 올바르게 동작하는 보정(offset 이 암전 수준을 뺌)은 화소를 움직이는 게 정상이라 **정상 실행이 "Input preserved: False" 로 표시**됐다. 소비처는 표시 두 곳(`MainWindow` 지표 문자열, `GuiE2eReportService` 보고서 줄)뿐이라 게이트를 속이지는 않았다. 필드를 `OutputIdenticalToInput` 으로 개명하고 표시 문구를 맞췄다(식은 그대로). **시험은 못 붙였다**: `ComputeMetrics` 는 `P/Invoke` 에 닿는 서비스 안의 private 정적 함수 셋이라 링크 없이 부를 수 없고, 개명은 컴파일러가 옛 이름의 잔존을 막는다(옛 이름 소비처는 0).

## 화면 확인 (고친 뒤, 항목 1·4)

같은 UIA 프로브(`after_tabs_uia.txt`, `after_run_selected_uia.txt`)를 고친 앱으로 다시 돌렸다. 211 의 값과 대비: 진단 탭 `smoke=Synthetic oracle pass; pass=True; latency=1.758ms`(전: `fail`), 표 `xpe_preprocess` **R3 "Synthetic oracle ready"**(전: R2), 요약 `Executable modules=1; levels R0=6, R2=1, R3=1`(전: `=0`, R2=2), 교정 탭에서 SWU-1.1 Run Selected 가 `blocked … fix synthetic oracle gaps` 대신 `Calibration validation: load the target raw image first.`(운영자가 할 다음 단계 — 모듈 게이트가 열림), 평가 탭 `Preprocess=ready` 유지.

## 반증 (`falsification_arms.txt`) — 8팔, 7팔은 의도한 시험이 빨강, 1팔은 아님(아래)

| 팔 | 결과 |
|---|---|
| 교정을 적재하지 않음(2026-09-10 이후의 실제 실패) | 오라클 시험 빨강 |
| defect 맵을 적재하지 않음 | 오라클 시험 빨강 |
| 사슬이 입력을 한 바이트 씀 | 오라클 시험 빨강 |
| 임시 폴더를 안 지움 | 임시 폴더 시험 빨강 |
| defect 맵이 아무 화소도 표시 안 함 | 오라클 시험 빨강 |
| 평가 탭을 `IsExportReady` 로 되돌림(211 의 모순) | E2E R02 빨강(R01 은 두 쪽이 같은 답이라 초록 — 이 상태가 모순을 드러내는 상태) |
| 준비도 함수가 `ProcessingEnabled` 를 무시 | 단위 시험 빨강 |
| **카드가 말한 "옛 시그니처로 되돌림"**(보정 델리게이트에서 `metadata` 인자를 뺌) | **빨강이 아니다**(3개 모두 통과) |

**마지막 팔이 빨강이 아닌 이유를 숨기지 않는다**: 인자를 하나 빼도 세 단계가 그대로 OK 였다 — 이 호출에서 네이티브가 셋째 인자(`metadata`)를 읽지 않았거나 읽어도 결과에 영향이 없었던 것으로 보인다(왜 그런지는 추적하지 않았다). 옛 오라클이 실패한 실제 원인은 인자 모양이 아니라 **교정 미적재**였고(첫 팔이 그것을 빨강으로 잡는다), 인자 수 불일치 자체는 이 시험이 못 잡는다. 인자 모양을 지키는 것은 헤더 대조(GUI-C-208 의 `NativeSignatureParityTests`)의 몫인데 그것은 xpe_common 만 보고 **xpe_preprocess 의 선언은 보지 않는다** — 열려 있는 공백이다(별도 카드 감).

## 예상: 229 M4 가 들어오면 운영자에게 보이는 것 (추정)

이 오라클의 세 교정 파일은 세션 ID 가 비어 있다(defect) 또는 모듈이 만든 값이다(offset·gain). M4 가 "세션 ID 가 빈 값/generated 인 것이 섞이면 혼합 상태당 1건 `XPE_WARN_CALIB_SESSION_UNSPECIFIED`" 라면 이 오라클의 적재 중 그 경고가 알림 큐에 1건 쌓일 수 있다 — 거부(CONFIG_INVALID)는 아니다(세션이 서로 *다른* 것이 아니므로). 운영자에게는 진단 탭 `Alerts: pending=N` 과 앱의 알림 목록에 그 문구가 보일 것이다. **M4 가 main 에 없어 확인하지 못했다**(추정이다). M4 병합 뒤 `after_tabs_uia.txt` 프로브를 다시 돌려 확인해야 한다. 같은 이유로 `IntegrationTests` 의 210 사슬 시험(세션 빈 defect 파일)도 그때 확인한다.

## CI 영향 (리더 소유 워크플로 — 패치 초안)

`ci.yml` 의 `gui-e2e-native` 잡은 gui 앱만 빌드한다. 새 E2E 가 레거시 앱을 띄우므로 거기서는 `ImageProcTest.exe was not found` 로 **건너뛰고**, 그 잡의 건너뜀 게이트가 빨개진다(의도한 반응 — 허용 목록 대신 빌드를 넣는다). `ci_legacy_app_build.patch.txt`(`git apply --check` 통과, YAML 로드 확인)가 그 잡에 `dotnet build clients/ImageProcTest/ImageProcTest.csproj -c Debug` 한 스텝을 더한다. Mock 잡(`gui-automation`)은 `XPE_NATIVE_DIR` 가 없어 두 시나리오를 건너뛴다(그 잡은 건너뜀 게이트가 없다). `dotnet-tests` 잡은 오라클 시험을 이미 돌린다(DLL 스테이징됨).

## 한계
- DLL 은 IntegrationTests 출력 폴더의 `xpe_preprocess.dll`(2026-09-26 파일). 이번 소스로 새로 빌드한 것이 아니다.
- 이미지를 불러 평가 탭의 `Run Selected` 로 실제 보정을 도는 것은 확인하지 않았다(이 카드는 준비도 판정의 일치를 다룬다).
- R02 의 "오라클이 못 도는 상태"는 TEMP 결함으로 만든 하나의 실패 유형이다(`Synthetic oracle exception`). 오라클이 *돌지만 실패*하는 상태(교정은 적재되나 결과가 틀림)는 E2E 로 만들 수 없어 오라클 시험의 변이 팔로만 확인했다.
- 오라클은 xpe_common 의 `XpeCommonApi` 타입을 링크로 공유한다. 앱 쪽 `XpeCommonApi` 와 네이티브 헤더의 일치는 별도로 보장되지 않는다(위 xpe_preprocess 헤더 대조 공백과 같은 맥락).
