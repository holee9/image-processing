# GUI-C-210 — REQ-GUI-IT-061: 보정 사슬 시험에 defect 단계와 입력 SHA-256 보존 단언 (#249)

증거(이 폴더): `falsification_arms.txt`, `oracle_vs_xunit.txt`. 변경: `clients/ImageProcTest.IntegrationTests/P1AReady/PreprocessCorrectionChainSmokeTests.cs` 한 파일(시험만, 제품 코드 변경 없음). IntegrationTests 전체 결과는 아래.

## 무엇을 바꿨나

- **사슬이 3단계**(offset → gain → defect)가 됐다. `RunCalibratedChain` 의 출력이 이제 defect 단계의 출력이므로, 기존 `CorrectionChain_RunTwice_DeterministicRmseIsZero`(RMSE 0)와 `CorrectionChain_Output_HasNoNanOrInf`(NaN/Inf 없음)가 **세 단계 사슬에서 그대로** 단언한다(시험 본문은 그대로, 대상만 3단계 출력).
- **defect 맵을 실제로 적재한다.** 이 모듈 API 에는 defect 맵 생성기가 없어서 시험이 XCal 파일을 직접 쓴다(`WriteDefectMapFile`: `xcal_format.h` 의 152바이트 헤더 — 매직 `XCAL`, 버전 1, 타입 DEFECT, UINT8_MASK, 16x16, 만료 없음, 세션 비움, config 0, 페이로드 256바이트, config‖payload 의 SHA-256 — 와 마스크). `xpe_calib_load_defect_map` 이 OK 를 돌려주는 것을 단언한다(적재 안 하면 단계가 CALIB_NOT_LOADED 를 낸다).
- **새 시험 셋.**
  - `CorrectionChain_DefectStage_CorrectsExactlyTheMarkedPixels_AndLeavesTheRestBitIdentical`: 입력의 표시 화소 둘(5,5)·(12,9) 에 핫 값 60000 을 넣고, defect 단계 뒤 그 화소가 **이웃 8개 값의 범위 안**에 있고(= 이웃에서 채움) 핫 값에서 5만 ADU 넘게 멀어졌으며, **나머지 254개는 gain 단계 출력과 비트 단위로 같다**(변경된 화소 목록 == 표시 화소 목록).
  - `CorrectionChain_InputBuffer_Sha256IsPreserved`: 사슬 전후 입력의 SHA-256 이 같고, "전" 해시가 **독립적으로 다시 만든 입력의 해시**와 같다(해시가 올바른 대상의 것임).
  - `Control_Sha256Hex_SeesAOneByteChange`(DLL 불필요): 해시가 한 바이트 변경을 본다 — 변하지 않는 해시는 제자리 쓰기 회귀를 통과시킨다.
- 핫 값을 넣은 이유: 기울기(ramp) 입력에서는 이웃 평균이 중심값과 같아서 보정해도 값이 안 변한다 — "결함 화소가 달라진다"는 단언이 성립하지 않는다. 핫 값이면 보정이 일어났는지 구별된다.

## 반증 (`falsification_arms.txt`) — 7팔 전부 의도한 시험이 빨강, 전부 바이트 동일 복원

| 팔 | 빨강 |
|---|---|
| defect 맵을 적재하지 않음 | 사슬을 도는 4개(defect·SHA·결정성·NaN) |
| defect 단계가 아무것도 안 함(출력 = gain 출력) | defect 단계 시험 |
| 맵이 핫 화소의 옆 화소를 표시 | defect 단계 시험 |
| 사슬이 입력을 한 바이트 씀 | SHA 보존 시험 (**리더 요구 반증**) |
| 해시가 상수 | 해시 대조 시험 |
| 표시 화소에 핫 값이 없음 | defect 단계 시험 |
| 맵이 기대보다 화소를 3개 더 표시 | defect 단계 시험 |

## 레거시 진단 오라클과의 대조 (항목 4) — 결과: **오라클이 현재 DLL 에서 실패한다**

오라클(`clients/ImageProcTest/Diagnostics/XpePreprocessSyntheticOracle.cs`)을 시험이 부르지는 않는다(지시대로). 대신 시험과 **같은 DLL**(`IntegrationTests/bin/.../xpe_preprocess.dll`)로 오라클을 한 번 돌렸다(오라클 파일과 그 의존 파일만 링크한 임시 콘솔, 저장소 밖; 결과 `oracle_vs_xunit.txt`).

| 속성 | 오라클 (임시 콘솔) | 새 xUnit | 같은 판정? |
|---|---|---|---|
| 전체 | `Passed=False` ("Synthetic oracle fail") | 통과 | **다름** |
| 입력 SHA-256 보존 | true | 통과 | 같음 |
| 출력 NaN/Inf 수 | 0 (출력이 전부 0 — Min=Max=0) | 0 | 값은 같으나 오라클의 0 은 **공허** |
| 결정성 RMSE | 0 (두 번 다 영 출력) | 0 (실제 보정 출력) | 값은 같으나 오라클의 0 은 **공허** |
| 단계 | offset·gain 모두 `CALIB_NOT_LOADED`, defect `INVALID_INPUT` | 세 단계 모두 OK | **다름** |

**원인(읽어서 확인한 것)**: 오라클의 델리게이트는 보정 함수를 **옛 시그니처** `(ref image, ref map)`·`(ref image, ref defectMap, config)` 로 부른다. 현재 헤더(`preprocess_api.h`)는 `(const XpeImageBuffer* input, XpeImageBuffer* output, const XpeImageMetadata* metadata)` 이고 교정 맵은 모듈 전역 저장소에서 읽는다(#117 결정 B). 오라클은 교정을 적재하지도 않는다. 그래서 세 호출이 전부 거부되고 출력은 0 으로 남으며, NaN/Inf 0·RMSE 0 은 "아무것도 안 돌았기 때문에 0"이다. 즉 요구가 "equivalent to RunChain"이라고 가리킨 참조 구현은 현재 DLL 에 대해 REQ-061 의 네 속성 중 두 개(NaN/Inf, 결정성)를 **공허하게** 통과시키고 전체 판정은 실패다. 새 xUnit 이 이 요구의 네 속성을 실제 보정 출력에서 처음 단언한다.
**부수 관찰(미확인 범위 명시)**: 레거시 앱의 `ModuleReadinessService`(114행)와 `NativeReadinessProbe` 가 `XpePreprocessReadinessProbe.Check()` 를 부르고, 그 프로브는 `IsSyntheticOracleReady: synthetic.Passed` 를 정한다. 위 오라클 결과로 보면 이 DLL 에서 그 값은 false 일 것이다. **앱을 실행해 화면의 표시를 확인하지는 않았다** — 코드 읽기와 오라클 단독 실행에서 이어진 추론이다. 제품 코드(clients/ 의 진단)는 이 카드 범위 밖이라 고치지 않았다. 별도 카드 감이다: 오라클을 새 시그니처와 교정 적재로 고치거나, xUnit 으로 대체하고 제거.

## 항목 5 (QA-A-229 M4) — **대기**
`XPE_WARN_CALIB_SESSION_UNSPECIFIED` 와 서로 다른 세션 맵의 `CONFIG_INVALID` 는 아직 main 에 없다(main 최근 12개 커밋과 소스에서 `SESSION_UNSPECIFIED` 검색 결과 0건). 병합 알림이 오면 이 카드의 사슬(세션 없는 offset/gain 파일 + 이번에 쓴 세션 비운 defect 파일)이 경고만 내고 거부되지 않는지 확인한다. **주의점**: 이번에 시험이 쓴 defect 파일은 세션 ID 가 비어 있다(`session_id` 64바이트 0) — M4 가 "세션 비어 있음/generated 섞임 = 경고"라면 경고 대상이고, "서로 다른 세션 = 거부"라면 세 파일이 모두 비어 있는 지금은 거부 대상이 아니다. 어느 쪽인지는 M4 의 규칙을 읽어야 확정되고, 확인하지 않았다.

## 한계
- Mock 구성: 이 시험들은 네이티브 `xpe_preprocess.dll` 이 없으면 기존 시험과 같이 건너뛴다(사유 "Skipped: xpe_preprocess.dll not staged"). `Control_Sha256Hex_SeesAOneByteChange` 만 DLL 없이 돈다.
- "이웃 범위 안"은 보정이 이웃에서 채운다는 계약(헤더 주석: 이웃 중앙값/평균)을 느슨하게 단언한 것이다. 정확한 값(평균인지 중앙값인지)은 단언하지 않는다 — 구현 세부가 바뀌어도 시험이 깨지지 않게 했다.
- defect 맵이 인접한 두 화소(클러스터)인 경우는 시험하지 않았다(고립 화소 둘).
