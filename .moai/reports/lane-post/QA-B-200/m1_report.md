# QA-B-200 M1 — QA-B-199 우선 결함 후보 여섯 개의 재현 확정 (#251)

카드: `.moai/lanes/post/inbox/QA-B-200.md`. 제품 코드는 바꾸지 않았다. 이 마일스톤에서 추가한 것은 시험 코드(모두 `DISABLED_`, 빌드만 되고 ctest 는 돌리지 않는다)와 증거다. 읽어서 찾은 후보를 실제로 실행해서 확인했다.

## 0. 결과 요약

| 후보 | 요구 | 재현 | 판정 | 영상 출력이 바뀌는가 |
|---|---|---|---|---|
| E1 USM 선명화 | REQ-ENH-018 | 재현됨 | **결함 확정**(코드가 018 식과 다름) | 예 — M2 는 레인 간 영향 결정 뒤 |
| C1 mAs 태그 | REQ-DICOM-009·015 | 재현됨 | **결함 확정** | 아니오(메타데이터) |
| C9 C-FIND 실패 상태 | REQ-DICOM-038 | 재현됨 | **결함 확정** | 아니오 |
| C7 C-STORE 실패·취소의 알림 | REQ-DICOM-032·039 | 재현됨 | **결함 확정**(취소는 성공으로 보고됨) | 아니오 |
| D5 `gsdfEnabled` 무시 | REQ-DISP-024 | 동작은 재현됨 | **결함 아님** — 요구 문구를 충족, 요구 정리 사안 | — |
| D9 표시 모듈의 예외·로깅 | REQ-DISP-030·031 | 031 은 재현됨, 030 은 해당 없음 | **031 만 결함 확정**, 030 은 결함 아님 | 아니오 |

재현 시험은 모듈별 `tests/test_repro_qa_b_200.cpp` 이고(CMake 등록), 실행 출력 전체가 `run_enhance.txt`, `run_display.txt`, `run_dicom.txt` 이다. 기존 시험은 영향이 없다: ci-post 의 관련 시험 312개 통과·0 실패, ci-dicom 전체 302개 통과·0 실패(`ctest_summary.txt`, CI 와 같은 `-E "Performance|Within…"` 필터).

## 1. E1 — USM 선명화

**요구.** REQ-ENH-018: `output[i] = input[i] + amount * (input[i] - blur(input)[i])` only where `abs(input[i] - blur(input)[i]) >= threshold`. REQ-ENH-021: 오버슈트는 `max(original * 2.0, original + amount * threshold)` 로 제한. REQ-ENH-020 은 `threshold` 가 음수일 때만 거부하므로 0 은 유효하다.

**코드.** `edge_enhance.cpp` 는 `max_add = amount * threshold` 로 두고 `sharpened` 를 `orig ± max_add` 로 양쪽 모두 자른다(내가 읽고 확인한 줄: `float hi = orig + max_add; float lo = orig - max_add;`).

**재현.** 기대값은 모듈에서 오지 않는다. 시험 안에 따로 쓴 분리형 가우시안 블러(σ = radius, 가장자리 복제)와 SPEC 식으로 계산했다. 모듈의 블러는 노출되지 않아 두 블러에는 작은 차이가 있지만 아래의 어긋남은 그보다 몇 배 크다. 64×64 수직 계단 영상, amount 0.5, radius 2, threshold 10(`run_enhance.txt`):

| 계단 높이 | 밝은 쪽 가장자리 화소의 변화량 — SPEC | 모듈 | 어두운 쪽 — SPEC | 모듈 |
|---|---|---|---|---|
| 100 | +20.013 | +5.000 | −20.013 | −5.000 |
| 1000 | +200.131 | +5.000 | −200.131 | −5.000 |

SPEC 의 변화량은 계단 높이에 비례해 10배가 되는데 모듈은 계단 높이와 무관하게 `amount*threshold = 5` 로 일정하다(비 `10.00` 대 `1.00`). threshold 0 에서는(amount 1, 계단 1000) SPEC 이 최대 400.263 을 바꾸는데 모듈은 0.000 을 바꾼다(전혀 무동작). 대조군: threshold 10·amount 1 에서는 모듈이 계단에서 10.000 만큼 바꾼다 — 하니스가 선명화를 볼 수 있고 모듈이 가장자리에서 아무것도 안 하는 것은 아니다.

**판정.** 코드의 양쪽 클램프는 REQ-ENH-018 의 식과 맞지 않는다(REQ-ENH-021 의 상한은 `max(2·orig, orig+5)` 이므로 018 의 결과 200 은 계단 1000 에서 021 도 어기지 않는다). 따라서 코드가 021 을 "더 좁게" 읽은 것이다. 이 결과는 SPEC 이 틀렸다는 뜻일 수도 있어서 M2 는 두 길 중 하나를 리더가 정해야 한다: (가) 코드를 018·021 문구대로 고친다, (나) SPEC 을 구현된 동작(`±amount*threshold`, threshold 0 은 무동작)에 맞춘다.

**영상 출력이 바뀐다.** (가)는 `xpe_edge_enhance` 가 가장자리에서 내는 값을 바꾼다. gui 의 화소 해시 시험과 기준 영상에 닿는지 리더가 정한 뒤에 M2 로 간다. (나)는 코드를 안 바꾸고 문서만 바꾼다.

## 2. C1 — mAs 태그

**요구.** REQ-DICOM-009: `(0018,1152) Exposure (mAs) --> outMeta->mAs`. REQ-DICOM-015 도 같은 태그를 쓴다.

**표준(`standards_cited.txt`).** PS3.6: (0018,1152) Exposure 는 VR `IS`, (0018,9332) Exposure in mAs 는 VR `FD`. DX 영상의 X-Ray Acquisition Dose 모듈에서 (0018,1152) 는 Type 3, "The exposure expressed in mAs, for example calculated from Exposure Time and X-Ray Tube Current." 이다(Innolitics 가 보여 주는 PS3.3 내용). (0018,9332) 가 어느 IOD 에 오는지는 읽지 못했다.

**재현(`run_dicom.txt`).** DCMTK 로 직접 만든 파일 — 모듈이 쓴 파일에서 (0018,9332) 를 지우고 (0018,1152) 를 IS "100" 으로 넣은 파일 — 을 모듈로 읽는다. 쓰기와 읽기가 같은 선택을 공유해 서로를 상쇄하지 못하도록 외부에서 만든 파일이다.

- 읽기: `module mAs = 0.000` (SPEC 은 100). 대조군: 모듈이 쓴 mAs 50 은 모듈이 50.0 으로 읽는다(기존 시험이 도는 왕복).
- 쓰기: 모듈이 mAs 100 으로 쓴 파일에 `(0018,1152)` 는 없고 `(0018,9332)` 가 `'100'`, VR `FD` 로 있다.

**판정.** 결함 확정. 다른 시스템이 쓴 (0018,1152) 를 mAs 0 으로 읽고, 이 모듈이 쓴 파일은 (0018,1152) 를 다른 읽기에 주지 않는다. 기존 왕복 시험이 이를 못 본 이유는 두 쪽이 같은 틀린 태그를 쓰기 때문이다.
**M2 설계 질문(리더).** (0018,1152) 는 정수 문자열(IS)이라 소수 mAs(예: 0.5, 2.5)를 담지 못한다. 1152 만 쓰면 정밀도를 잃는다. 1152 를 반올림해 쓰고 9332(FD)를 정밀값으로 함께 쓸지, 읽기만 둘 다 받을지의 결정이 필요하다.

## 3. C9 — C-FIND 실패 상태

**요구.** REQ-DICOM-038: C-FIND 가 실패하거나 시간 초과면 `XPE_ERR_NETWORK_FAILED`.
**표준.** PS3.4 Table K.4-1: `A700` Refused: Out of resources 는 Failure 이고, "SCU 는 표의 범위 안의 모든 실패 코드를 표에 적힌 실패 의미로 인식해야 한다".
**재현.** 모의 SCP 가 C-FIND 에 상태 `0xA700` 으로 답하게 했다(시험 헬퍼 `mock_scp.hpp` 의 `forcedFindStatus`, 기본 0 이라 기존 동작은 그대로). 대조군: 같은 SCP 를 강제 없이 쓰면 항목 3개가 돌아온다. 결과: `rc 0 (Success), outJson '[]'`.
**판정.** 결함 확정. 실패한 질의가 "일치 없음"과 구별되지 않는 `XPE_OK` 와 `[]` 로 돌아온다. 의사 접수 시스템이 거부했는데 호출자는 "대기자 없음"으로 읽는다.

## 4. C7 — C-STORE 실패와 취소의 알림

**요구.** REQ-DICOM-032: C-STORE 실패 시 `XPE_ERR_NETWORK_FAILED` 와 실패 이유가 담긴 WARNING 알림. REQ-DICOM-039: 취소된 작업은 알림 메시지에 취소 표시가 있는 `XPE_ERR_PROCESSING_FAILED`.
**재현(`run_dicom.txt`).**
- 실패: SCP 가 `0xA700` 으로 답하게 함 → `rc -10 (Network operation failed)`, **알림 0건**. 반환 코드는 맞고 알림이 없다. 대조군: 강제 없는 전송은 `XPE_OK`.
- 취소: SCP 가 요청을 받은 뒤 3000 ms 기다리게 하고, 전송이 시작된 지 800 ms 에 `xpe_dicom_cancel()` 을 보냈다. 호출은 **3017 ms 뒤에 `rc 0 (Success)` 로 돌아오고 알림 0건**이다.
**판정.** 결함 확정. 실패한 전송은 알림 없이 끝나고, 더 나쁘게 취소는 무시될 뿐 아니라 성공으로 보고된다 — 호출자는 취소가 먹혔는지 전송이 끝났는지 구별할 수 없다. 취소가 두 점검점에서만 보이고 플래그가 호출 진입에서 지워진다는 QA-B-199 의 읽기와 일치하는 실행 결과다.

## 5. D5 — `gsdfEnabled` 무시

**요구.** REQ-DISP-024: "WHEN `gsdfEnabled` is non-zero in the params, the system SHALL apply the GSDF-calibrated LUT entries".
**재현(`run_display.txt`).** `xpe_gsdf_calibrate` 로 만든 LUT 를 플래그 1 과 0 으로 같은 영상(64 화소)에 적용: `IDENTICAL output (0 of 64 pixels differ)`. 대조군: 항목은 적용된다(1.0 → `lutData[1023]`, 0.0 → `lutData[0]`).
**판정.** 동작(플래그가 아무것도 바꾸지 않음)은 재현되었지만 **결함이 아니다.** 요구는 플래그가 0 이 아니면 보정된 항목을 적용하라고만 말하고 0 일 때를 정하지 않으며, 코드는 어느 쪽에서도 항목을 적용하므로 문구를 충족한다. 플래그 0 은 기존 시험이 자기 LUT 를 적용하는 정상 방법이다(`make_identity_lut`, `make_constant_lut`, `lutData[0]=999` 가 그대로 출력되는 시험 등, `test_presentation_lut.cpp:44-59,98-107`). "0 이면 건너뛴다"로 고치면 REQ-DISP-020 시험이 깨진다. 권고: 요구 문구를 "이 플래그는 정보용" 으로 정리하거나 플래그를 없앤다(리더·SPEC 소유).

## 6. D9 — 표시 모듈의 예외와 로깅

**REQ-DISP-030 (예외를 경계 밖으로 던지지 않음) — 결함 아님.** 표시 모듈 소스에는 던질 수 있는 구문이 없다: `throw`, `new`, `std::vector/string/map/unique_ptr`, 던지는 변환이 모두 검색에서 0건이고, 유일한 동적 할당은 `std::malloc`(실패하면 `XPE_ERR_OUT_OF_MEMORY` 로 반환)이다. `try/catch` 가 없는 것은 던질 것이 없어서다(`d9_req030_031_grep.txt`). 요구는 "던지지 않는다"이며 지금 그렇다. 이는 미래에 던지는 구문이 추가되면 보호가 없다는 취약성이지 현재의 결함은 아니다. QA-B-199 의 "예외 처리 부재" 후보 중 이 부분은 철회한다.
**REQ-DISP-031 (진입·종료 DEBUG 로그, 오류 ERROR 로그) — 결함 확정.** xpe_common 의 로그 파일(`xpe_log_set_file`, 레벨 1 = DEBUG)에 대조군 줄을 하나 쓰고 표시 함수 다섯 개(성공 2·오류 3)를 호출했다. 로그 파일은 62바이트, 대조군 줄 1회, **표시 함수를 가리키는 줄 0**이다. 소스에서도 `xpe_log`, `spdlog`, `LOG` 호출이 0건이다. CMake 는 spdlog·fmt 를 PRIVATE 로 링크하지만 아무것도 쓰지 않는다.

## 7. 레인 간 영향과 M2 순서(제안)

- **먼저 고칠 수 있는 것(영상 출력이 안 바뀜): C1, C9, C7.** C1 은 mAs 정밀도 질문(§2)을 정해야 하고, C9·C7 은 순수한 SCU 동작이다. 셋 모두 dicom 레인 소유 파일이다.
- **레인 간 영향을 정해야 하는 것: E1.** `xpe_edge_enhance` 출력이 바뀐다(길 (가)). gui 의 화소 해시·기준 영상에 닿는다.
- **코드 수정이 필요 없는 것: D5(요구 정리).** D9 는 REQ-DISP-031 만 코드(로깅 추가) 대상이고 출력은 안 바뀐다.
- 결함으로 확정된 4건 + D9 일부에는 `#251` 이 맞는 추적 항목이다.

## 8. Gap / 잔여 위험

Gap(관측하지 않은 것)
- (0018,9332) 가 어느 IOD 에 오는지는 읽지 못했다(카드의 "어느 IOD 에 어느 것이 오는지"의 후반). PS3.6 의 이름·VR·VM 만 인용했다.
- C-STORE 응답 상태 표(PS3.4 Annex B)는 읽지 않았다. C7 은 `0xA700` 하나로 재현했고, REQ-DICOM-032 는 코드를 지정하지 않는다.
- E1 의 기준 블러는 내가 시험 안에 쓴 것이라 모듈의 블러와 같지 않다. 어긋남(5 대 20, 5 대 200)은 그 차이보다 훨씬 크지만, 정확한 일치값을 주장하지는 않는다.
- C7 취소는 3초 지연하는 모의 상대 하나로 재현했다. 실제 PACS 의 지연 형태(연결 단계, 데이터 전송 단계)를 구분해 시험하지는 않았다.
- 재현 시험은 모두 `DISABLED_` 이고 한 번씩만 실행했다(반복성은 보지 않았다). C7 취소는 시간에 의존한다.

잔여 위험
- 모의 SCP 헬퍼(`mock_scp.hpp`)에 `forcedFindStatus`·`forcedStoreStatus`·`storeDelayMs` 를 더했다. 기본값 0 에서 동작이 그대로임은 기존 `test_dicom_network_scu` 가 ci-dicom ctest 302개 안에서 통과한 것으로 확인했다.
- 표시·enhance 재현 실행 파일이 ci-post 에서 만들어지므로 빌드 시간이 조금 늘어난다.
