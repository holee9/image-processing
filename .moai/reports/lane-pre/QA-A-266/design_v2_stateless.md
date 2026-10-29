[2026-10-17 20:46 KST] QA-A-266 설계 v2 — 무상태 `xpe_verify_offset_signed` (리더 결정 20:44 반영; `design_signed_offset_quality.md` 를 이 한 쪽이 고친다)

**바뀐 것**: 세대 번호, `XpeStageDiagnostics` 40바이트 판, 오류 코드 −18 은 채택하지 않는다. 함수는 **원시 다크와 offset 맵 자체를 인수로** 받고 아무 전역 상태도 읽지 않는다(`xpe_preprocess_init` 불필요, 스레드 안전). 호출자가 맵을 직접 넘기므로 "같은 맵"은 계약상 자명하다. 맵의 SHA-256·크기를 결과에 되돌려 기록한다.
**바뀌지 않은 것**: UINT16 출력과 0 클램프, 기존 `xpe_verify_offset`·`XpeCalibrationMetrics` 불변, 지표별 클램프 전/후 표(§5), ROI 규칙(원시의 어두운 10분위, 엄격한 `<`, 최소 1 %), 측정 비트, 부등식 시험 조건과 반례 +0.2, 반증 실행기 touch 규칙.

**API (2개)**
- `xpe_verify_offset_signed(raw_image, offset_map, quality)`: `raw_image` UINT16, `offset_map` FLOAT32(같은 폭·높이).
- `xpe_verify_offset_signed_file(raw_image, offset_xcal_path, quality)`: 위와 같되 맵을 `.xcal` 에서 읽는다(헤더 SHA·형식 검증은 기존 리더, **만료는 검사하지 않는다**: 저장된 맵의 품질을 재는 것이지 영상에 적용하는 것이 아니다).
- 잔차 `x_i = float(raw_i) − map_i`(float32 빼기, 클램프 전). 출력 `o_i = (uint16)(clamp(x_i, 0, 65535) + 0.5f)` = `xpe_offset_correct` 와 같은 연산.

**구조체 `XpeOffsetQuality` (120 바이트, 8바이트 정렬)**: 머리 32 바이트 `structSize, writtenSize, validMask, roiPixels, pixelCount(u64), mapWidth, mapHeight` → 오프셋 32 `mapSha256[32]`(맵 payload 의 float32 바이트열의 SHA-256) → 64 `darkBiasSigned, dsnuSigned, darkReductionDb, clampRatePct, zeroOutputRatePct, darkBiasOutput, dsnuOutput`(각 double). 규칙: `structSize ≥ 32`, 작은 구조체는 지역에 채운 뒤 `min(structSize, 120)` 만 복사, 그룹 비트는 그 그룹의 마지막 필드 끝이 `structSize` 안일 때만 켠다, 큰 `structSize` 는 앞 120 바이트만 쓴다, 판 올림은 끝에 덧붙이기. 비트: `ROI_SIGNED 0x1`(→ 구조체 80), `DARK_REDUCTION 0x2`(88), `CLAMP_RATE 0x8`(104), `ROI_OUTPUT 0x4`(120), `MAP_IDENT 0x10`(64). `CLAMP_RATE` 는 입력이 유효하면 항상 켜진다.

**오류(기존 코드만 사용)**: NULL → `XPE_ERR_INVALID_INPUT`(쓰지 않음); `structSize < 32` → `INVALID_INPUT`(쓰지 않음); 원시 또는 맵의 형식 틀림 → `XPE_ERR_UNSUPPORTED_FORMAT`; 폭·높이 0·오버플로·`dataSize` 부족·맵 크기 ≠ 영상 크기 → `INVALID_INPUT`; 맵에 비유한 값 → `XPE_ERR_INVALID_CALIB_DATA`; `.xcal` 읽기 실패 → 리더의 코드; 메모리 → `XPE_ERR_OUT_OF_MEMORY`; 예외 → `XPE_ERR_PROCESSING_FAILED`. 오류 때는 머리 32 바이트만 0 으로 쓴다(`structSize ≥ 32`일 때). ROI 부족·상수 원시는 `XPE_OK` 이고 ROI 비트만 꺼진다.

**구현·시험·실측**: 같은 번역 단위(`xpe_verify_metrics.cpp`)에 구현해 옛 함수의 `compute_mean`/`compute_std` 를 그대로 쓴다 → `darkBiasOutput`·`dsnuOutput` 이 옛 함수와 비트 단위로 같다. 시험은 설계 §6 의 T-A~T-E 와 ①~⑪ 중 무상태가 된 것(맵 SHA·크기 반환, 파일 변형과 버퍼 변형의 동일성, ABI 크기별, 오류표 각 행, 백분위 대조, 옛 함수 연속성)이다. 실측: `Dark_08~16` 을 `clean_set_v1/offset.xcal` 로 새 지표와 옛 지표 나란히, `Dark_16` −4.0542 재현. 반증 실행기는 변이 적용 뒤 헤더를 쓰는 소스를 touch 하고 시험 실행 파일 수정 시각이 변이 시각 이후임을 단언한다.
