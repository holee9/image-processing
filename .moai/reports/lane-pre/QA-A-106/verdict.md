# QA-A-106 — SHA-256 을 Windows CNG 로(선택지 A), 파일 형식과 SRS 대조 (#179, #188)

커밋 `6f06266` (dev/preprocess, 미푸시, 기준 main `d330fe6`)

## 1. 주장

1. `Sha256Stream` 의 구현을 `xpe_sha256_backend.cpp` 로 옮기고, Windows 에서는 CNG(`BCryptHash*`)를 쓴다. 제공자가 열리지 않거나 호출이 실패하면 PicoSHA2 를 쓴다. `xpe_sha256_backend_name()` 이 어느 쪽인지 알려 준다.
2. 이 기계에서는 `cng` 경로를 탄다(시험 로그).
3. 교정 3파일 적재가 약 475 ms → **약 130 ms** 가 되었다. **요구 200 ms 안에 들어온다.**
4. 다이제스트는 바뀌지 않는다. PicoSHA2 로 해시한 파일이 그대로 검증된다.
5. #188: 실제 파일에는 CRC-32 가 없고 SHA-256 32바이트가 있다. SRS 네 항목의 서술과 다르다(표는 §4). 코드는 바꾸지 않았다.

## 2. 증거

### 2.1 적재 시간 (3072², 준비 3회 제외, 중앙값, N=25, 3회 실행)

| 파일 | QA-A-105 후(PicoSHA2) | QA-A-106(CNG) |
|---|---|---|
| offset 37.7 MB | 201.9 / 193.2 / 207.4 | 56.2 / 59.4 / 58.1 |
| gain 37.7 MB | 208.5 / 209.3 / 228.6 | 56.7 / 59.0 / 62.5 |
| defect 9.4 MB | 62.8 / 61.5 / 64.9 | 15.0 / 15.8 / 16.4 |
| **3파일 합계** | 약 475 ms | **약 130–137 ms** |

- 요구(SRS-CALIB-PERF-003) 200 ms 대비 약 65–69% 다.
- 분해: 같은 측정 프로그램의 "manual" 부분은 **PicoSHA2 로 컴파일된 참조 경로**다(측정 프로그램은 헤더의 인라인 코드를 자기 안에 담고 있어 DLL 교체의 영향을 받지 않는다). offset 기준 PicoSHA2 168.3 ms 대 API 전체 59.4 ms 이므로, CNG 해시는 대략 API(59.4) − 읽기(30.4) − 할당·복사(11.0) ≒ **18 ms** 다(37.7 MB → 약 2 GB/s). PicoSHA2 는 약 190 MB/s 였다.
- 측정 중 다른 레인 부하: 측정 전 CPU 27%, GUI 레인 `dotnet` 4개. 컴파일러·링커 없음.

### 2.2 다이제스트 동일성
시험 `test_sha256_backend_parity.cpp` 5건, 모두 통과(`a106-run-parity.log`).
- 백엔드 이름 기록: `SHA-256 backend: cng`.
- 길이 0, 1, 55, 56, 63, 64, 65, 127, 128, 1000, 1 MiB, 1 MiB+3 에서 PicoSHA2 를 **직접 호출한 값**과 바이트 동일.
- 청크 1·64·4096·1 MiB 로 나눠 먹여도 한 번에 먹인 값과 동일(3 MiB+517 입력).
- `compute_sha256_two_parts(config, payload)` 가 이어 붙인 입력의 해시와 동일.
- PicoSHA2 로 해시한 헤더를 손으로 써서 만든 파일이 `xpe_calib_load_offset` 으로 검증을 통과.
- 실파일 대조: 적재 결과를 다시 저장한 파일의 페이로드가 CNG 전후로 바이트 동일(`cmp -i 152`).
- 외부 검증: 저장된 파일을 파이썬 `hashlib.sha256` 으로 계산한 값이 헤더의 32바이트와 일치.

### 2.3 빌드·시험
- `BUILD_EXIT=0`, warning C 0.
- 전처리 `ctest --preset ci-preprocess` 686건 통과, 종료 0(`a106-ctest.log`).
- 링크: `xpe_preprocess`, `xpe_calib_fixture_gen`, `xpe_preprocess_tests` 에 `bcrypt` 를 연결했다. 세 대상이 헤더의 인라인 함수를 쓰므로 백엔드 `.cpp` 도 함께 컴파일한다(심벌은 DLL 에서 내보내지 않는다).

## 3. 기준 귀속
- 기기·빌드는 QA-A-102~105 와 같다(i7-12700, `build/ci-preprocess` RelWithDebInfo).
- 수정 전 값은 QA-A-105 의 측정(같은 프로그램, 같은 기계)이다.

## 4. #188 — 파일 형식과 SRS 대조 (판독)

실제 파일(`loaded_offset_cng2.xcal`, 37,748,888 바이트)의 처음 152바이트를 읽어 확인했다.

| 항목 | SRS 서술 | 실제 (코드 + 파일 바이트) |
|---|---|---|
| 매직 | `magic=0x585045`(FUNC-001) | `"XCAL"` 4바이트 = `58 43 41 4C`(`xcal_format.h:100`) |
| 헤더 크기 | 34바이트(FUNC-001, FUNC-002) | **152바이트**(`static_assert`, 파일에서 확인) |
| 무결성 값 | CRC-32 4바이트, 다항식 0x04C11DB7(FUNC-001), "CRC-32 checksum"(SAFE-003), "CRC-32 validation shall be incremental"(PERF-003) | **SHA-256 32바이트**, 헤더 오프셋 120–151. 대상은 `config_json ‖ payload`(`xcal_format.h:112`, `xcal_reader.cpp`) |
| 위치 | 헤더 뒤(FUNC-001: header + CRC + pixel data) | 헤더 **안**(마지막 32바이트) |
| 손상 시 오류 | `XPE_ERR_IO_FAILED`(FUNC-001, SAFE-003) | `XPE_ERR_CONFIG_INVALID`(`xcal_reader.cpp` 해시 불일치·헤더 검증). `XPE_ERR_IO_FAILED` 는 읽기 실패에 쓴다 |
| 페이로드 | offset 은 uint16(FUNC-001) | offset 은 **float32**(`XCAL_FMT_FLOAT32`, 3072×3072×4 = 37,748,736 바이트) |
| 게인 값 범위 | [0.1, 10.0] 밖이면 `XPE_ERR_INVALID_CALIB_DATA`(FUNC-002) | 적재 코드에 범위 검사가 없다(검색 범위: `xpe_calib_load_gain.cpp`). `XPE_ERR_INVALID_CALIB_DATA` 코드도 `xpe_error.h` 에 없다 |
| 만료 | expiryEpochMs(FUNC-001) | `expiry_epoch_ms` 헤더에 있음. 동작 일치 |
| 증분 계산 | "calculated during read, not post-hoc"(PERF-003) | QA-A-105 에서 읽으며 해시하도록 바꿨다. 알고리즘만 SHA-256 이다 |
| CRC-32 존재 여부 | — | `xpe_crc32()` 공개 API 와 `calibration_manager.cpp` 의 CRC-32 구현은 있다. XCal v1 파일에는 쓰이지 않는다 |

- 즉 SRS 네 항목이 적은 파일 형식은 **현재 형식이 아니다**(헤더 크기·매직·무결성 알고리즘·오류 코드·화소 형식이 모두 다르다).
- 카드 지시대로 코드는 바꾸지 않았다. SRS 정리는 lead 가 한다.

## 5. 미검증
- CNG 가 SHA-NI 를 쓰는지는 확인하지 않았다(측정된 약 2 GB/s 는 그 수준이다).
- Windows 외 환경에서 폴백 경로를 실행해 보지 않았다(코드상 `_WIN32` 가 아니면 PicoSHA2).
- CNG 호출이 중간에 실패하는 경로(메모리 부족 등)는 만들어 보지 않았다. 그 경우 다이제스트가 0 이 되어 검증이 실패한다(틀린 값을 통과시키지 않는 방향).
- 압축(RLE) XCal 파일로는 시간을 재지 않았다.
- `xpe_preprocess_pipeline` 의 프레임당 적재 경로 시간은 이 카드에서 다시 재지 않았다.

## 6. 잔여 위험
- 해시가 빨라져 적재의 비중이 읽기(30 ms/파일)와 복사로 옮겨 갔다. 디스크가 느린 환경에서는 결과가 달라진다(SSD 기준 측정).
- CNG 제공자는 프로세스에서 한 번 열고 닫지 않는다. 프로세스 종료 시 OS 가 회수한다.
- 백엔드가 다르면 성능 특성이 달라지므로, 성능 기준을 다시 잡을 때는 `xpe_sha256_backend_name()` 을 함께 기록해야 한다.
