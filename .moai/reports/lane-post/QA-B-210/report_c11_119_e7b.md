# QA-B-210 — C11 (C-STORE 전송 구문 하나), #119 낮음 (.204 오류 절), E7b (근거 문구 정정)

기준: `dev/postprocess` fc768122 위. Refs #251. 사용자 결정은 확정(#251 코멘트), 다시 묻지 않음.

## C11 — C-STORE 는 파일의 전송 구문 하나만 제안
**재현(수정 전, HEAD)**: MockScp 가 받은 연관 요청의 제안 목록을 구조적으로 캡처(`lastProposals()`, 요청 자체에서 읽음)해 단언하는 시험 3건이 **모두 빨강** (`c11_red.txt`) — 제안이 항상 Explicit LE · J2K Lossless · Implicit LE 세 개였다.
**수정**(`DicomNetworkSCU.cpp`): 파일 메타 `(0002,0010)` 하나만 제안, 변환 없음. 메타가 없는 맨 데이터셋은 DCMTK 가 로드할 때 감지한 구문(`getOriginalXfer`)을 쓰고, 둘 다 없으면 제안할 것이 없어 `XPE_ERR_DICOM_INVALID`(카드가 정하지 않은 경계라 내가 정함 — 리더 확인 요청). 상대가 구문을 거절하면 수락된 컨텍스트가 없어 `negotiateAssociation` 이 실패 → `XPE_ERR_NETWORK_FAILED`, C-STORE 는 시도되지 않는다. 헤더(`dicom_api.h`)에 반환값과 노트 추가.
**시험**(`test_dicom_network_scu.cpp`, 22/22): Explicit LE 파일 → 제안 목록은 그 구문 하나 / J2K 파일(`xpe_dicom_write_j2k`) → J2K 하나 / 진짜 Explicit VR **Big Endian** 파일(mock 은 Explicit LE·Implicit LE·J2K 만 수락) → `NETWORK_FAILED`, 연관 요청은 1건, 제안은 그 구문 하나, `storeRequests` 불변(C-STORE 미시도). 반증: HEAD 의 SCU 로 되돌리면 3건 빨강(`c11_red.txt`), 수정 후 22/22(`c11_green.txt`). (첫 시도의 거절 시험 픽스처는 메타만 JPEG Lossless 로 바꾼 파일이라 DCMTK 가 로드를 거절해 IO_FAILED 였다 — 진짜 Big Endian 파일로 고치고 HEAD 에서 다시 빨강을 확인했다.)

## #119 낮음 — `.204` 오류 문구의 표준 절
**재현**: 새 시험 `JpipPixelDataErrorCitesThePs35ClauseOfTheTransferSyntax` 가 옛 검증기에서 빨강(`p119_red.txt`: `.204` 의 메시지가 `PS3.5 A.6`).
**수정**(`DicomValidator.cpp`): 절을 구문별로 — `.94`/`.95` → A.6, `.204` → A.11, `.205` → A.12 (헤더에 이미 적힌 근거와 일치). 시험은 `.94`→A.6, `.204`→A.11(A.6 아님)을 단언. `.95`·`.205` 는 이 DCMTK 빌드가 파싱하지 못해(KnownDivergence 시험) 단언하지 못했고, 매핑은 같은 코드 줄에 있다. 검증기 전체 32/32(`p119_green.txt`).

## E7b — E7 의 근거 문구 정정 (코드 동작 불변)
Codex #151 이 맞다: 7.5 를 넘으면 반경만 15 로 고정되고 공간 가중치 `exp(-0.5 d²/σ²)` 는 요청한 σ 를 계속 따른다(거리 10 에서 σ=7.5 면 0.411, σ=8 이면 0.458 — 계산해 확인). 처음 적은 "7.5 처럼 동작한다·지정값이 무시된다" 는 틀렸다.
정정 문구: **"7.5 는 2σ 범위가 반경 15 안에 다 들어가는 최댓값이다. 7.5 를 넘는 값은 가중치는 σ 를 따르지만 반경이 2σ 보다 작게 잘려 매개변수가 요구하는 가우시안 모양이 아니다."** 고친 곳: `noise_reduce.cpp` 상수 주석과 검증 주석, `enhance_basic_api.h` `@return` 문구, `test_noise_reduce.cpp`·`test_exception_guard.cpp` 시험 주석, E7 보고서(`report_e7.md`, 정정 이력 표기). 동작은 그대로(상한 7.5, 초과 INVALID_INPUT).

## 증거
| 주장 | 산출 | 관측 |
|---|---|---|
| C11 수정 전 빨강 3건 | `c11_red.txt` | 3 FAILED |
| ci-dicom 전체 (C11 + #119) | `c11_119_ctest_dicom.txt` | 380/380 passed |
| ci-post 전체 (E7b: 주석만 변경) | `e7b_ctest_post.txt` | 1451/1451 passed |
| #119 수정 전 빨강 | `p119_red.txt` | 새 시험 FAILED ("A.6" 인용) |
| Doxygen | `c11_doxygen.txt` | exit 0 |

## Gaps
- C11: 메타에 `(0002,0010)` 이 없는 맨 데이터셋의 경계(감지 구문 → 없으면 DICOM_INVALID)는 시험하지 않았다. 기존 시험 `CStoreFileWithoutSopUids_FallsBackToDxDefault`(메타 없는 Explicit LE 파일)가 감지 경로를 지나가 통과한다.
- #119: `.95`/`.205` 의 절(A.6, A.12)은 이 DCMTK 빌드에서 시험할 수 없다.
- 요구 문구(REQ-DICOM-029, REQ-ENH-007/020)는 리더 몫.

## Residual risk
- 이전에 J2K 로 고정 제안하던 호출자가 있다면(파일 구문과 무관하게 변환해 보내던 경로는 없었지만) 이제 파일 구문이 피어에 없으면 NETWORK_FAILED 이다 — 사용자 결정이다.
