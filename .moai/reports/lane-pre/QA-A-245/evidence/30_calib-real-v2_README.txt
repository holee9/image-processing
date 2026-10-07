calib-real-v2  --  운영자 앱이 읽는 1단계 실데이터 보정 세트 (QA-A-245, Refs #245)

무엇인가
  기준 영상 v2 (m1-baseline-v2) 를 만든 것과 같은 offset / gain / 결함 맵을, 앱이 읽는 고정 이름 세 파일로 내보낸 것이다.
  앱(gui/ImageProcTest)은 보정 디렉터리에서 offset.xcal, gain.xcal, defect.xcal 만 읽는다. 세 디렉터리 설정을 이 폴더로 가리키면 된다.

파일 (SHA-256)
  offset.xcal  37,748,950 B  0f888eb6958057706289c8d8d30d63badf4132aab2db850b94988d2bc723ba82
  gain.xcal    37,749,092 B  004056fd2a3fb89a67a1255a8be1454f071e29d115a74eb70c90eca3d32fd142
  defect.xcal   9,437,336 B  ff9df014b3b66898d987d1298855f35669445951d8958e7c7d2858240a4bc007

만든 방법 (제품 생성기와 제품 기록기, 하니스는 시험 A241Measure.DISABLED_A245_ExportAndAppPath)
  1. xpe_calib_generate_offset(dark.raw 1 프레임, 100 ms, 25 C)
  2. xpe_calib_generate_gain(bright04, bright05, bright06, 다크 참조 없음)  [선량 가중 ADU 평균]
  3. 결함 맵 = BPMap.map (23,505 화소) 을 XCal 결함 파일로 감쌈
  4. 세 파일이 한 세션 ID 를 갖게 하고(gain 은 offset 의 것, 결함 파일은 같은 값으로 기록) 만료를 설정:
     expiry_epoch_ms = 1822893017654 (2027-10-07 07:10:17 UTC, 만든 시각 + 365 일)
  명령 (bash 한 줄, 리포 루트에서):
    XPE_A240_CAL=<repo>/tests/test_data/CalData_6 XPE_A240_WRIST=<repo>/gui/ImageProcTest/fixtures/gui-s0/raw/wrist_lat_3072x3072.raw
    XPE_A245_OUT=D:/workspace-github/xpe-data/calib-real-v2 XPE_A245_REF=D:/workspace-github/xpe-data/m1-baseline-v2/wrist_lat_3072x3072_corrected_v2_f32le.raw
    build/ci-preprocess/bin/xpe_preprocess_tests.exe --gtest_also_run_disabled_tests --gtest_filter="A241Measure.DISABLED_A245_ExportAndAppPath"
  코드: dev/preprocess 의 이 카드 커밋 (보고서 .moai/reports/lane-pre/QA-A-245/report.md 에 SHA).

입력 (SHA-256)
  tests/test_data/CalData_6/dark.raw      2ece967e7b379ec8bc1d54cdabd3c4740a08c8a643d0cb3e6ad4702b12f7621b
  tests/test_data/CalData_6/bright04.raw  5fd9392f467c6e6de28d846a4c1d5e75b005d71641be75ca104125c0ae215cb6
  tests/test_data/CalData_6/bright05.raw  2fc44324ab68c3003de9a6c7ad95c47e8c19653b7a4c3724ac17333f45c23552
  tests/test_data/CalData_6/bright06.raw  6938260f1372859cfb584d3c658cff25576e3d8772f10cc4f0cdcd0e33ccc136
  tests/test_data/CalData_6/BPMap.map     5122aa6e27282b5ca675a8f43c48049d3dc3cf0c37fc9b11b88ad8a81f85b2d0
  gui/ImageProcTest/fixtures/gui-s0/raw/wrist_lat_3072x3072.raw  c823233f2196a217f4512bfec47e62a17f003cc894ef7947e1f40dd0b59a3d70

만료를 1 년으로 한 이유
  생성기가 쓴 만료는 0 (만료 없음)이다. 0 이면 적재할 때마다 XPE_WARN_NO_EXPIRY 경고가 나고 앱 화면에 알림이 섞인다. 1 년은 사용자가 이 세트로 확인하는
  동안 만료가 끼어들지 않으면서 0 이 아닌 값이다. 만료가 지나면 보정 단계가 XPE_ERR_CALIBRATION_EXPIRED 로 프레임을 거부한다 (정상 동작). 그 뒤에 쓰려면
  위 명령으로 다시 만들면 된다.

재현성
  파일 바이트는 다시 만들면 달라진다 (헤더의 생성 시각과 자동 생성되는 세션 ID 가 다르다). 맵의 내용(화소 값)과 그것으로 만든 보정 영상은 바이트 단위로 같다.
  세션 ID 는 생성기가 만든 값이라 "미지정 세션" 경고(XPE_WARN_CALIB_SESSION_UNSPECIFIED)가 앱에 보일 수 있다 (세 파일이 같은 값을 가져서 충돌은 없다).

앱 경로 대조 결과 (자세히는 보고서)
  이 세트를 앱과 같은 순서(load offset, gain, defect -> xpe_offset_correct -> xpe_nonlinearity_correct -> xpe_gain_correct -> xpe_defect_correct)로 실영상에
  적용한 float32 출력은 기준 영상 v2 (sha256 d152e8ebba2cba142a95ba7ba981354a393fa18551b46572d8b1c3f36e8fdfd4)와 9,437,184 화소 모두 바이트 동일하다.
  앱이 화면에 보이는 영상은 이것을 16 비트로 줄여(ScaleToUInt16) 그린 것이라, 화면의 값은 이 float 값과 다르게 보이는 것이 정상이다.

한계
  검출기 1 대(CalData_6)의 데이터다. 평탄 4·5·6 이 한 촬영 조건이라는 것은 추정이다 (선량·kVp·mAs 기록이 데이터에 없다).
  bright* 는 이미 오프셋 보정된 값이라 gain 은 다크를 뺀 값이 아니라 bright 그대로의 평균으로 만들었다 (다크 이중 차감 없음, 보고서 §2).
  이 세트는 1단계 확인용이고 임상 검증된 보정이 아니다.
