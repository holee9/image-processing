# QA-A-25 검증 보고서 — 미등록 테스트 3종 이관·등록 (#120)

- 레인: Lane A (pre) / 브랜치 `dev/preprocess`
- 카드: QA-A-25 (`.moai/lanes/pre/inbox/QA-A-25.md`)
- 관련 커밋: `e63fbe5`, `f6ca334`, `ab43f7b`, `2156f94`, 본 커밋

## 1. 주장 (Claim)

1. 디스크에만 있고 `XPE_TEST_SOURCES` 에 등록된 적 없던 3개 스위트를 현행 ABI 로 이관하고 전부 등록했다.
   - `test_golden_reference.cpp` (20 케이스)
   - `test_gain_correct_reciprocal_fma.cpp` (22 케이스)
   - `test_calibration_roundtrip.cpp` (12 케이스)
2. 이관 과정에서 문서/테스트가 아니라 **구현이 정답인 계약 충돌 2건**을 구현 기준으로 확정했다.
   - 게인: `gain_correct.cpp:296` 은 저장된 맵으로 **나눈다**. 골든 16개 값을 `raw / S` 로 재계산했다.
   - 리드아웃: `xpe_validate_readout_artifact` 는 0..100 점수가 아니라 **bool 2개**(dropped / nonuniform)를 낸다.
3. 캘리브레이션 왕복 스위트는 은퇴한 map-as-argument API 전체를 파일·전역 저장소 기반 현행 API 로 옮겼고, 그 과정에서 **문서와 다른 실제 반환코드**를 실측으로 확정했다.
   - 페이로드 손상 → SHA-256 불일치는 `XPE_ERR_IO_FAILED` 가 아니라 `XPE_ERR_CONFIG_INVALID` (`xcal_reader.cpp:210-211`).
   - `xpe_calib_check_expiry` 는 만료된 파일에도 `XPE_OK` 를 돌려주고 만료 여부는 출력 인자로 전달한다.
   - `xpe_calib_save` 는 항상 `expiry_epoch_ms = 0` 을 쓴다(`xpe_calib_save.cpp:56`). 만료 왕복은 save 로 표현할 수 없어 XCal 파일을 직접 기록해 검증했다.
4. `CMakeLists.txt:150-168` 의 낡은 주석("두 계약은 스펙 결정이라 이관 불가")을 실제 결정 내용으로 교체했다.
5. ci-preprocess 전체 스위트 **476/476 PASS**, 회귀 0건.

## 2. 증거 (Evidence)

빌드 + 전체 ctest (vcvars64 → `cmake --build --preset ci-preprocess` → `ctest --preset ci-preprocess --output-on-failure`):

```
477/477 Test #477: P1A066IoTest.T2_LoadOffset_DirectoryPath_ReturnsIoFailed ...............   Passed    0.34 sec

100% tests passed, 0 tests failed out of 476

Total Test time (real) =  32.47 sec
```

로그 전문: `.moai/reports/lane-pre/QA-A-25/a25-b9.log` (exit=0).

중간 실패 1건과 그 원인 — 픽스처 이름 충돌 (실측 출력):

```
All tests in the same test suite must use the same test fixture
class.  However, in test suite GenerateOffsetTest,
you defined test SingleFrameMeanEqualsFrame and test SingleFrame_OutputEqualsInput
using two different test fixture classes.
```

`test_xpe_calib_generate_offset.cpp` 가 이미 `GenerateOffsetTest` 를 쓰고 있었다. 본 파일의 픽스처를 `RoundtripGenerateOffsetTest` 로 개명해 해소했다 (`.moai/reports/lane-pre/QA-A-25/a25-b8.log` → 같은 디렉터리 `a25-b9.log`).

## 3. baseline 귀속

- 직전 baseline: 동일 프리셋 `ctest --preset ci-preprocess` **464/464 PASS** (`.moai/reports/lane-pre/QA-A-25/a25-ctest3.log:934`), HEAD `ab43f7b` 시점.
- 이번 실측: **476/476 PASS**. 증분 +12 는 이번 커밋에서 등록한 `test_calibration_roundtrip.cpp` 12 케이스와 정확히 일치하며, 기존 464건 중 실패로 돌아선 것은 없다.
- 두 수치 모두 이 워크트리에서 같은 명령으로 직접 측정한 값이다. 다른 시점·다른 트리의 숫자를 옮겨 오지 않았다.

## 4. 미검증 (Gaps)

- **ci-common 프리셋은 이번 턴에 재실측하지 않았다.** 변경은 `modules/preprocess/**` 에만 닿았고 common 헤더·소스는 건드리지 않았으나, "영향 없음" 은 추론이지 측정이 아니다.
- **ASan 재측정 없음.** `build/asan-a17` 트리에서 이 3개 스위트를 돌려본 적이 없다. 새로 등록된 케이스가 파일 I/O 와 전역 저장소를 오가므로 누수·해제 오류가 있어도 이번 실측으로는 드러나지 않는다.
- **커버리지 수치 미측정.** QA-A-15 에서 0%/6% 로 관측됐던 `pipeline.cpp` / `rle_codec.cpp` / `calibration_cache.cpp` 가 이번 등록으로 얼마나 올라갔는지 숫자로 확인하지 않았다.
- **C# 테스트 미확인.** Lane C 소유라 손대지 않았고 실행도 하지 않았다.
- `xpe_calib_generate_offset` 의 SigmaClip 경로(#97)는 이번 스위트가 기본 설정만 통과시키므로 검증 범위 밖이다.

## 5. 잔여 위험 (Residual risk)

- **임시 파일 경합.** 세 스위트 모두 `fs::temp_directory_path()` 또는 작업 디렉터리에 XCal 파일을 쓴다. 이전 카드에서 크래시가 남긴 잔여 파일 탓에 `std::rename` 이 실패해 `-9 IO_FAILED` 가 재현된 사례가 있다. `TempFile` 소멸자가 `.tmp` 형제 파일까지 지우도록 했지만, 프로세스가 중간에 죽으면 여전히 남는다.
- **만료 일수 경계.** `ExpiryTimestampPreservedRoundtrip` 은 잘림 나눗셈 때문에 89/90 을 모두 허용한다. 시스템 시계가 테스트 도중 크게 조정되면 흔들릴 수 있다.
- **전역 저장소 순서 의존.** 새 스위트는 케이스마다 모듈 초기화·해제를 수행한다. 다른 스위트가 전역 캘리브레이션을 남긴 채 끝나면 영향을 받을 수 있으나, 이번 실행 순서에서는 관측되지 않았다.
- 픽스처 이름 충돌은 링크 타임이 아니라 **런타임**에만 드러난다. 앞으로 추가되는 스위트도 같은 함정을 밟을 수 있다.

## Card Cross-Check

| 마일스톤 | 카드 |
|---|---|
| golden_reference 이관·등록 | QA-A-25 |
| gain FMA 스위트 이관·등록 | QA-A-25 |
| calibration_roundtrip 이관·등록 | QA-A-25 |
| CMakeLists 낡은 주석 정리 | QA-A-25 |
| ASan / 커버리지 재실측 | 신규 카드 필요 |

## 인용 로그 (같은 디렉터리)

| 파일 | 내용 |
|---|---|
| `a25-b9.log` | 최종 빌드 + 전체 ctest, 476/476 PASS (exit=0) |
| `a25-b8.log` | 픽스처 이름 충돌로 12건 실패한 직전 실행 |
| `a25-ctest3.log` | 직전 baseline 464/464 PASS (934행) |
| `a25-ctest.log`, `a25-ctest2.log` | 카드 중반 회귀 확인 실행 |
| `a25-b1..b7.log` | golden/FMA 이관 중 빌드 실측 |
| `a25-golden.log`, `a25-fma.log` | 해당 스위트 단독 실행 결과 |
