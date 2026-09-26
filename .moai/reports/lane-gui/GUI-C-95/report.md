# GUI-C-95 보고 — 처리에 닿지 않는 설정의 비활성·"미적용" 표시와 연결 조사 시험 (#182)

- 기준: `dev/gui` `6fde6fc` 위에 커밋 `8f340b3` (미푸시)
- 증거 디렉터리: `build/e2e-c95/`

## 1. 주장

1. 처리에 닿지 않는 설정 컨트롤이 비활성이고 "미적용" 표시가 붙었다. 이유는 툴팁에 있다: "처리 연결 없음 — #182". 값과 설정 파일 키는 바꾸지 않았다.
   - Calibration Stages 7종: 라디오 버튼 21개
   - Lane B Overrides 2개: 입력 2개와 Reset 버튼
   - **카드에 없던 대상** — Lane A/B 알고리즘 선택 콤보박스 2개. 연결 조사 시험에서 드러났다(3절).
2. 연결 조사 시험 `SettingsProcessingConnectionTests` 가 추가됐다. 바인딩된 설정은 모두 다음 셋 중 하나여야 하고, 아니면 빨강이다.
   - Real 처리 경로에서 읽힘
   - 미연결 목록에 있고, 값을 바꾸는 컨트롤이 모두 비활성
   - 화면 상태 목록에 있음
3. 대조군 3종을 원본 파일을 직접 약화시켜 돌렸고, 셋 다 빨강이었다. 시험 안의 대조군 4건은 늘 돈다.
4. E2E `UnappliedSettingsScenarios` 3건이 실제 앱에서 비활성·표시·툴팁을 읽는다. Mock과 Native 모두 통과했다. 앱의 그룹 하나를 다시 켜면 빨강이 된다.

### 교정 경로 입력 확인 결과

| 설정 | 처리에서 쓰이는가 | 근거 | 화면 컨트롤 | 조치 |
|---|---|---|---|---|
| `OffsetCalibrationDirectory` | 쓰임 | `RealXpeBackend.cs:269` 에서 `GuiPreprocessRunner.Run` 으로 넘기고, 그 안의 `Path.Combine(offsetDirectory, OffsetFile)`(`GuiPreprocessRunner.cs:44`)이 경로로 쓴다 | 없음. 찾아보기 명령(`MainWindowViewModel.cs:100`)을 참조하는 XAML이 0건 | 변경 없음 |
| `GainCalibrationDirectory` | 쓰임 | `:270` → `:45` | 없음 | 변경 없음 |
| `DefectCalibrationDirectory` | 쓰임 | `:271` → `:46` | 없음 | 변경 없음 |

- 이 세 값을 바꾸는 경로는 자동화 인자(`MainWindow.xaml.cs:81-83`)와 픽스처 매니페스트(`GuiFixtureManifestService.cs:63-65`)뿐이다.
- Mock(`MockXpeBackend.cs:52-54`)과 `RawImageLoader.cs:87-89` 는 이 값들을 로그와 요약에만 쓴다.

### 바인딩된 설정 전체 (시험이 모은 24개)

| 판정 | 설정 |
|---|---|
| Real 처리에서 읽힘 (4) | `VoiLutMode`, `VoiWindowCenter`, `VoiWindowWidth`, `SelectedBodyPart`(래퍼 경유) |
| 미연결 + 비활성 (11) | 교정 모드 7, `LaneBSharpeningSigma`, `LaneBDenoiseStrength`, `LaneAAlgorithm`, `LaneBAlgorithm` |
| 화면 상태 (9) | `ComparisonMode`, `ComparisonOverlayOpacity`, `ComparisonPanX`, `ComparisonPanY`, `ComparisonSwipePosition`, `ComparisonZoomScale`, `ShowDisplayPanel`, `AnalysisTab`, `FocusMode` |

합계: 4 + 11 + 9 = 24. 수집 결과에서 목록 두 개를 뺀 나머지이며, 시험의 개수 단언은 전체(24)와 교정 모드 바인딩(21)을 고정한다.

## 2. 증거

**빌드**
- `build-final-int.txt`: 통합 테스트 빌드 `BUILD_EXIT=0`, 경고 0 / 오류 0
- `build-e2e.txt`: `BUILD_EXIT_GUI=0`, `BUILD_EXIT_E2E=0`

**시험 결과**

| 실행 | 결과 | 파일 |
|---|---|---|
| 통합 전체 | 실패 0 / 통과 216 / 건너뜀 1 (C-93 때 209 + 새 시험 7) | `full-int-final.txt` |
| Mock E2E 전체 | 실패 0 / 통과 91 / 건너뜀 2 (C-93 때 88 + 새 시나리오 3). 건너뜀은 이전과 같은 IB-02, NativeProvenance | `full-mock.txt`, `full-mock.trx` |
| Native, U-01~03 + E01d | 통과 4. 상태 줄 `mode=Native … src=bin` | `u-native.txt` |

**대조군 (원본을 직접 약화)**

| 약화 | 결과 | 출력 |
|---|---|---|
| F1: 미연결 목록에서 `LaneBDenoiseStrength` 삭제 후 재빌드(`BUILD_EXIT=0`) | 실패 1 | `LaneBDenoiseStrength: bound in … via 'LaneBDenoiseStrength', not read by Real processing, and not declared unconnected` |
| F2: `AnalysisPanel.xaml` 에 `<TextBox Text="{Binding Settings.LastRunSetId}" />` 추가 | 실패 1 | `LastRunSetId: bound in … via 'Settings.LastRunSetId', not read by Real processing, and not declared unconnected` |
| F3: `CalibrationStagesGroup` 의 `IsEnabled="False"` 삭제 | 실패 1 | `OffsetCorrectionMode: declared unconnected but the control … is enabled` |
| FE: `LaneBOverridesGroup` 의 `IsEnabled="False"` 삭제 후 앱 재빌드(`BUILD_EXIT=0`), E2E U-02 | 실패 1 | `U02 LaneBSharpeningSigmaInput enabled=True` |

- 복원한 뒤 시험은 다시 통과했다(`restored.txt`).
- 앱은 다시 빌드했고(`fe-restore-build.txt`, `BUILD_EXIT=0`), XAML은 최종본과 바이트가 같은 것을 `cmp` 로 확인했다.
- 로그는 `f1-list-entry-removed.txt`, `f2-new-binding.txt`, `f3-group-enabled.txt`, `fe-u02.txt` 에 있다.

**시험 안에 상시 들어 있는 대조군**
- `Control_NaiveSearch_CountsTheSummaryAsProcessing`: 요약 헬퍼를 따라가고 문자열을 남기는 단순 검색은 교정 모드 7개를 "읽힘"으로 센다. 실제 분석은 세지 않는다.
- `Control_RemovingAnUnconnectedEntry_IsRed`: 미연결 목록의 11개를 하나씩 빼 보면 모두 빨강이다.
- `Control_BindingANewUnreadSetting_IsRed`, `Control_EnabledControlForUnconnectedSetting_IsRed`
- `ProcessingSearch_FindsKnownReads`: 존재 대조군. 로더를 따라가야 닿는 `RawWidth` 까지 찾는다.
- `Collection_FindsTheKnownBindings`: 수집이 아무것도 못 찾아 통과하는 경우를 막는다(24개, 교정 모드 바인딩 21개).

**E2E 관측 (Mock)**
- 라디오 21개가 모두 `enabled=False` 이고, 단계마다 저장값 하나(`Auto`)가 선택된 채 보인다.
- 표시는 `name='미적용' offscreen=False` 이고, 툴팁(`help=`)이 `#182` 를 포함한다.
- 대조군: 같은 패널의 `VoiWindowCenterInput` 은 `enabled=True` 다.
- 시그마 입력은 비활성이지만 `0.85` 를 보여 준다. 알고리즘 콤보박스는 `Production v1.2` / `Candidate v1.4` 를 보여 준다.

## 3. 기준과 판단 (카드와 다르게 한 것)

1. **"처리 경로"를 Real 백엔드로만 정했다.** 카드에는 "Real 또는 Mock" 이라고 되어 있다. 그러나 Mock 은 교정 모드 7종을 합성 픽셀에 적용한다(`MockXpeBackend.cs:95` → `:125` `CreateMockCalibrationPixels`, `ShouldApplyStage`).
   - Mock 을 넣으면 7종이 "연결됨"이 되어 리더 결정(#182: 미연결)과 정반대가 된다.
   - 미연결 목록에 올린 값을 처리가 읽으면 빨강으로 만든 점검("목록이 거짓말하지 않기")도 성립하지 않는다.
   - 시험 주석에 이유를 적었다.
2. **화면 상태 목록을 세 번째 범주로 두었다.**
   - 비교 뷰포트 상태 6개, 표시 패널, 분석 탭, 포커스 모드는 백엔드로 갈 값이 아니다.
   - 카드의 이분 규칙대로라면 모두 빨강이 된다.
   - 명시 목록이므로 새 바인딩이 여기 조용히 들어갈 수는 없다(F2 참고).
3. **Lane A/B 알고리즘 선택도 비활성으로 했다.**
   - 리더 결정의 원칙("처리에 닿지 않는 설정은 비활성+미적용")을 그대로 적용한 확장이다.
   - 되돌리려면 콤보박스 두 개의 `IsEnabled`·표시와 목록 두 줄을 지우면 된다.
   - `ViewportShell.xaml:257,284` 의 라벨은 여전히 알고리즘 이름을 보여 준다. 표시 전용이라 그대로 두었다.
4. **표시 전용 바인딩은 비활성 검사에서 뺐다.**
   - 쓰기 가능한 바인딩만 검사한다: 명시 `TwoWay`/`OneWayToSource`, 또는 기본이 양방향인 WPF 속성(`TextBox.Text`, `ComboBox.SelectedItem`, `IsChecked` 등).
   - 다른 네임스페이스의 사용자 정의 컨트롤은 보수적으로 쓰기 가능으로 본다.

## 4. 미검증

- **수집이 놓치는 것**:
  - 코드 비하인드에서 만든 바인딩
  - `<Binding Path=…/>` 요소 문법과 MultiBinding
  - `=> Settings.P` 가 아닌 모양의 래퍼
  - 명령으로 설정을 바꾸는 경로(현재는 교정 경로 찾아보기 명령인데, XAML에서 참조하지 않는다)
- **비활성 판정 방식**: 시험은 XAML의 문자 그대로 `IsEnabled="False"` 만 본다. 바인딩·스타일·코드로 비활성이 되는 경우는 시험이 보지 못하고, E2E 가 대신 본다.
- **요약·로그 구별의 한계**:
  - 보간 문자열이 아닌 로그 호출(예: `AddLog(settings.P.ToString())`)은 "처리"로 센다. 현재 진입점에는 그런 읽기가 없다(검색 범위: `RealXpeBackend.cs`, `RawImageLoader.cs` 의 진입 메서드와 따라간 메서드).
  - 문자열 제거는 직접 만든 스캐너로 한다. 원시 문자열(`"""`)은 처리하지 않는다. 두 파일에는 그런 문자열이 없다.
- **따라가지 않는 경계**: `settings.X` 로 넘긴 값이 호출된 쪽에서 실제로 쓰이는지는 따라가지 않는다. 교정 경로 3개는 `GuiPreprocessRunner.cs:44-46` 을 직접 읽어 확인했다.
- **Native 실행 범위**: 새 시나리오와 E01d 만 돌렸다. Native E2E 전체는 돌리지 않았다.
- **툴팁 문구**: 저장 여부와 백엔드 전달 여부만 적었다. 픽셀 연쇄 결정(#182 이후) 뒤에는 문구를 다시 봐야 한다.

## 5. 잔여 위험

- **목록 유지 비용**: 설정 하나를 처리에 연결하면 미연결 목록에서 빼야 한다. 빼지 않으면 "listed but read" 빨강이 알려 주는데, 이것은 의도한 동작이다.
- **메서드 선언 정규식**: 진입 메서드의 선언 모양이 바뀌면(예: 식 본문 안의 여러 줄 람다) 본문 추출이 달라질 수 있다.
  - 메서드를 찾지 못하면 빨강이 된다(`processing method … was not found`).
  - 본문을 잘못 자르는 경우까지 막지는 못한다.
- **Mock 사용자에게 보이는 차이**: Mock 에서는 교정 모드를 바꾸면 합성 영상이 달라졌다. 이제는 컨트롤이 비활성이라 Mock 으로 그 차이를 볼 수 없다. 리더 결정의 결과이며, `UnappliedSettingsScenarios` 밖의 E2E 소스에서 교정 모드·Lane B 값·알고리즘 선택을 다루는 코드를 검색한 결과 0건이다(`clients/ImageProcTest.E2ETests/**/*.cs`).

## Card Cross-Check

| 항목 | 카드 |
|---|---|
| Calibration Stages / Lane B 비활성·미적용 | GUI-C-95 |
| 교정 경로 확인 표 | GUI-C-95 |
| 연결 조사 시험 + 대조군 | GUI-C-95 |
| E2E 확인 | GUI-C-95 |
| Lane A/B 알고리즘 선택 비활성 (확장) | GUI-C-95 — 리더 판정 필요 |
| 설정을 실제 처리에 연결 | 범위 밖 (픽셀 연쇄 결정 뒤) |
