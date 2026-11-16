[2026-10-23 17:06 KST] [xpe-gui] GUI-C-255 1단계 실행 사실 (Refs #278)

최종 증거 폴더: `run-20261023-165511/`. 관측 도구: `clients/ImageProcTest.GuiObservation/`.

실행 명령: `final-run.ps1`이 `dotnet clients/ImageProcTest.GuiObservation/bin/RelWithDebInfo/net8.0-windows/ImageProcTest.GuiObservation.dll D:/workspace-github/xpe-gui <최종 증거 폴더> <set> <96 또는 144>`를 아래 순서로 하나씩 실행했다. 창 조작은 UIA Invoke/ExpandCollapse/Toggle/Value/SelectionItem 패턴만 사용했다.

| 순서 | 세트 | 배율 / 실제 DPI | 증거 하위 폴더 | 도구 / 앱 종료 코드 | GUI/CLI 바이트 비교 | 각 파일 길이 |
|---|---|---|---|---|---|---:|
| 1 | s0_gf | 100% / 96 | s0_gf-uia3-165520 | 0 / 0 | equal=true | 31,457,280 |
| 2 | s1_um300 | 100% / 96 | s1_um300-uia3-165633 | 0 / 0 | equal=true | 31,457,280 |
| 3 | s0_gf | 150% / 144 | s0_gf-uia3-165753 | 0 / 0 | equal=true | 31,457,280 |
| 4 | s1_um300 | 150% / 144 | s1_um300-uia3-165904 | 0 / 0 | equal=true | 31,457,280 |

- 단계: 앱 시작 → View의 Calibration Paths Panel → Offset/Gain/Defect 각각 Browse로 XCal 폴더 선택 → File/Open Raw로 RAW 열기 → Pipeline/Run Preprocessing → Metrics → File/Save Corrected Image(float32) → File/Exit.
- 모든 최종 실행에 `SAVED_EXISTS=True`, `EXIT=0`, `CLI_COMPARISON_WRITTEN`, `OWNED_PROCESS_REMAINING=0` 기록이 있다.
- 3072×2560 UInt16LE 원본 RAW를 각 새 증거 폴더에 복사한 뒤 GUI에서 그 복사본을 열었다. 저장은 앱의 제안 파일명 `input-001_3072x2560_corrected_f32le.raw`로 같은 새 폴더에 했다. 원본 폴더에 저장하지 않았다.
- GUI와 CLI는 같은 `build/gui-c255-native/bin` DLL 및 같은 세트 XCal을 읽었다. CLI는 UI 결과를 재사용하지 않고 `xpe_preprocess_pipeline_out`을 직접 호출했다. 구성: `{"bypassTemp":true,"bypassNonlinearity":true,"bypassBinning":true}`. SHA와 실제 config, flags, 파일 경로는 각 `bit-comparison.json`에 있다.
- s0_gf: GUI/CLI SHA-256 `F1724B40D8691F8F1B6F5B13C81B53AD1EA36E52B0CDE86D275A9C4E9190E1D5` (두 DPI 동일).
- s1_um300: GUI/CLI SHA-256 `76BD1D1CFFFB4783AB8DF821E817E18353C110B3506D28DC26F2F0B1A8487AB6` (두 DPI 동일).
- 배율은 Windows Display Settings의 `SystemSettings_Display_Scaling_ItemSizeOverride_ComboBox`를 UIA로 조작했다. 종료 finally에서 원래 100%로 복원했고 `RESTORE_SCALE_EXIT=0`, `display-restored-100.json`의 선택 `100%(권장)`을 확인했다. 해상도/장비 설정은 바꾸지 않았다.

## 비교 도구의 음성 대조

`--compare`에 s0_gf RAW/XCal과 **다른 세트인 s1_um300 GUI 저장물**을 의도적으로 조합했다. 두 결과는 길이가 같지만 바이트가 다르다. 새 `negative-control/bit-comparison.json`의 `equal=false`, 도구 종료 **1**, 원문 `CLI_COMPARISON_ERROR=GUI/CLI bytes differ; see bit-comparison.json`을 확인했다. 기존 결과를 덮어쓰거나 변경하지 않았다. 실제 네 실행의 비교 결과와 이 음성 대조를 구분한다.

## 원본과 입력 범위

manifest `D:/workspace-github/xpe-2g-offline/batch-20261022-1230/manifest.json`의 s0_gf/s1_um300에 기록된 보정 자료와 RAW **52개**를 SHA-256로 다시 대조했다. 불일치 **0개**. 원본 RAW001 해시는 각 비교 JSON에도 기록했다. XCal은 `pre-w3072-20261022-1240/<set>/xcal`에서 읽었다. `.par`/`.dcm` 열기와 2G DLL 실행은 하지 않았다.

## 관찰의 한계

도구 종료 0과 바이트 일치는 화질/화면 사용성/규제 적합성 합격 판정이 아니다. PNG 본문 흰색, Metrics 자리표시자, 작은 UIA 글꼴 및 미지원 글꼴은 2단계 보고서에 별도로 기록한다. 독립 QA/Reviewer/리더의 판정은 남아 있다.
