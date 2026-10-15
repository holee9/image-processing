# QA-B-211 — dicom 시험 스위트의 병렬 실행 충돌 (#257)

기준: `dev/postprocess` 09f9cd55 위. Refs #257. 제품 코드는 건드리지 않았다(시험 6개 파일 + 작은 헤더 하나).

## 1. 원인 (측정)
고치기 전 `ctest -j 8`: **106건, 102건 실패 / 380** (`before_j8_1.txt`, `before_j8_2.txt`). 스위트별: DicomReaderTest 94, DicomWriterTest 4, DicomJ2kFailureTest 4, DicomNetworkTest 2, DicomValidatorTest 2.
실패 메시지 종류(첫 실행): `Expected equality` 678줄(결과), `remove_all: ... being used by another process` **58**, `Env235().ok` 24, `ff.loadFile(src).good()` 10, `ff.loadFile(donor).good()` 6, `ff.saveFile(out).good()` 7, `remove_all: directory is not empty` 3, `remove_all: Access is denied` 2.

| 가설 | 맞다면 보일 흔적 | 확인 |
|---|---|---|
| 고정 임시 폴더 + 시험 프로세스마다 SetUp/TearDown 의 `remove_all` | `remove_all` 오류, 다른 프로세스가 만든 파일이 사라져 `loadFile`/`saveFile` 실패 | **맞음.** `grep temp_directory_path()`: reader·network·validator·writer·writer_inner·param_dependency 6개 파일이 고정 문자열 폴더(`xpe_dicom_reader_test` 등)를 쓰고 reader/network/validator/writer/writer_inner 가 `remove_all` 한다. `gtest_discover_tests` 가 시험마다 별도 프로세스를 만들어 각각 SetUpTestSuite 를 실행하므로 폴더를 동시에 만들고 지운다. 대조군: 이미 PID 를 쓰는 `test_dicom_failure_paths`(`xpe_failpaths_<이름>_<PID>`)와 209b 의 로깅 시험은 실패 목록에 없다 |
| 고정 포트 | `bind`/`connection` 오류, 같은 포트 | 아님. `mock_scp.cpp` 의 `probe_free_port()` 가 0 바인딩으로 포트를 고른다. `11112`/`11113` 은 시험 안에서 곧바로 덮어쓰이는 기본값이고 `19999` 는 "아무도 듣지 않는 포트" 목표다(연결 거부를 기대). 실패 메시지에 포트 오류 없음 |
| 전역 상태(DLL) | 로그/알림 상태가 시험 사이에 새어 나감 | 프로세스가 분리돼 있어 DLL 전역은 시험끼리 공유되지 않는다. 증거 없음 |
| 작업 디렉터리 | 상대 경로 파일 못 찾음 | `WORKING_DIRECTORY` 는 모듈 소스로 같게 고정됨. 증거 없음 |

## 2. 수정
`test_pid.h`(새, 시험 전용): `xpe_test::pid_suffix()` = `"_<PID>"`. 6개 파일의 스크래치 폴더에 붙였다 — `xpe_dicom_reader_test_<PID>` 등. 한 프로세스 안에서는 안정이라 같은 fixture 의 시험들은 폴더 하나를 그대로 공유하고, 프로세스끼리만 갈라진다(209b 의 로그 파일명과 같은 방식). **`RUN_SERIAL` 도 `RESOURCE_LOCK` 도 쓰지 않았다**: 공유가 필요한 자원이 없었다. 포트는 이미 0 바인딩이라 바꿀 것이 없었다.

## 3. 증거
| 주장 | 산출 | 결과 |
|---|---|---|
| 고치기 전 `-j 8` (대조) | `before_j8_1.txt`, `before_j8_2.txt` | 106, 102 실패 / 380 |
| 고친 뒤 직렬 | `after_serial_build.txt` | 380/380 passed |
| 고친 뒤 `-j 8` x5 | `after_j8_summary.txt`, `after_j8_1..5.txt` | 매번 **380/380 passed**, 5회 모두 시험 수 380 (줄지 않음) |
| 시험 수 | 위 모든 실행 | 380 그대로 |

## Gaps
- `ctest -j` 이외의 병렬(여러 ctest 를 동시에 띄움)에서는 같은 PID 가 아니므로 폴더는 갈라지지만, 같은 포트 프로브의 경쟁(probe_free_port 의 알려진 레이스)은 남아 있다. 시험하지 않았다.
- `-j 8` 이상(예: 16)이나 부하가 큰 기계에서의 결과는 보지 않았다. 5회 통과가 비결정적 충돌의 부재를 증명하지는 않는다.
- 이 변경으로 CI(직렬)의 동작은 달라지지 않는다.

## Residual risk
- 시험 도중 프로세스가 비정상 종료하면 `xpe_dicom_*_<PID>` 폴더가 임시 폴더에 남는다(PID 별이라 쌓일 수 있음). 이전에는 같은 이름 하나를 덮어썼다.
