# QA-A-36 검증 보고서 — `xpe_calib_fixture_gen` 결정적 픽스처 생성 CLI

- 레인: Lane A (`xpe-pre`), 브랜치 `dev/preprocess`
- 커밋: `96d0428` (CLI), `ddfefe2` (테스트)
- 증거 디렉터리: `.moai/reports/lane-pre/QA-A-36/`
- Refs #141 #120

---

## 1. 주장 (Claim)

| # | 주장 | 판정 |
|---|------|------|
| C1 | 전처리 모듈의 **공개 생성기**(`xpe_calib_generate_offset` / `xpe_calib_generate_gain` / `xpe_bpm_generate`)만으로 offset·gain·defect XCal 세트를 만드는 CLI 가 존재하고 정상 종료한다 | PASS |
| C2 | 같은 `--seed` 로 두 번 실행하면 세 파일 모두 **바이트 동일**하다 | PASS |
| C3 | 다른 `--seed` 는 offset·gain 을 바꾼다 (defect 는 주입 좌표 고정이라 의도적으로 불변) | PASS |
| C4 | 산출 디렉터리를 `xpe_preprocess_pipeline` 의 `calibPath` 로 그대로 넘겨 1프레임이 통과한다 | PASS |
| C5 | `--expiry-ms` 가 헤더에 실려 `xpe_calib_check_expiry` 로 과거/미래 양방향 왕복한다 | PASS |
| C6 | 새 export 를 만들지 않았다 | PASS |
| C7 | ci-preprocess 전체 재측정에서 회귀 없음 | PASS |

---

## 2. 증거 (Evidence)

### C1 · C2 · C3 — CLI 실행과 결정성 (`a36-cli-run.log`)

명령: `build\ci-preprocess\bin\xpe_calib_fixture_gen.exe --out <dir> --width 256 --height 256 --seed <n>`

```
===== RUN 1 =====
offset.xcal  0581ce02cfb58b70cff2fe09ddba468c22f7ea42c2d8ffd742a833ed414d24b4
gain.xcal    4b6e178c920ca8b2e24e5a2493fbe29b29c025e1008bc82dc48e1b78d1829ccd
defect.xcal  1609b6211015b60543da43ea0bf490abc8201ee5949d8ed8f159846cc92ca846  (4 defect pixels)
exit=0
===== RUN 2 (same args) =====
offset.xcal  0581ce02cfb58b70cff2fe09ddba468c22f7ea42c2d8ffd742a833ed414d24b4
gain.xcal    4b6e178c920ca8b2e24e5a2493fbe29b29c025e1008bc82dc48e1b78d1829ccd
defect.xcal  1609b6211015b60543da43ea0bf490abc8201ee5949d8ed8f159846cc92ca846  (4 defect pixels)
exit=0
===== RUN 3 (different seed) =====
offset.xcal  9b8b5cfb112146110a8665aa230d4f4926967b0df64d1e57e168ca1dab110b13
gain.xcal    cf5f1e1a59fa9c6e71f8000545aa935ded1f4863e41c0d11a00d84a3a213af22
defect.xcal  1609b6211015b60543da43ea0bf490abc8201ee5949d8ed8f159846cc92ca846  (4 defect pixels)
exit=0
===== EXPIRY =====
offset.xcal  8d1b0ae4d0591517f1883ac3542f5bf120d33c72947ae84890424c51fe04c95c
gain.xcal    8c311469f10e9eab639e7a21e35010a34d38284301a25327c97c8cd9d58d6b2a
defect.xcal  d79a9b0603469cdac825d975181345a06774205d5fedb7a4430ddf1298eebe89  (1272 defect pixels)
exit=0
```

RUN 1 과 RUN 2 의 세 SHA 가 전부 일치한다. RUN 3 은 offset·gain 만 달라진다.
이 로그에는 `xpe_bpm_generate returned -1` 경고가 없다 — 실제 불량화소 검출기가
돌았다는 뜻이다(초기에는 `XpeBpmConfig cfg{}` 0-초기화 때문에 거절당했다).

### C2 RED/GREEN — 감도 측정

RED (`a36-red.log`): `created_epoch_ms` 를 벽시계 값으로 되돌리고 재빌드.

```
offset.xcal differs between two runs with the same seed
gain.xcal differs between two runs with the same seed
defect.xcal differs between two runs with the same seed
[  FAILED  ] CalibFixtureGenTest.SameSeedProducesIdenticalBytes (5114 ms)
[  FAILED  ] CalibFixtureGenTest.DifferentSeedChangesOffsetAndGain (5064 ms)
[  PASSED  ] 3 tests.
```

GREEN (`a36-green.log`): 원복 후 재빌드.

```
[  PASSED  ] 5 tests.
```

즉 이 테스트는 "고정 타임스탬프" 라는 실제 설계 결정에 붙어 있고, 그것이
무너지면 실제로 붉게 변한다. (`DifferentSeed...` 도 RED 에서 함께 깨진 것은
그 케이스가 defect 파일의 **동일성**을 명시적으로 고정하기 때문이다.)

### C4 · C5 — 파이프라인 수용과 만료 왕복 (`a36-green.log`)

```
[ RUN      ] CalibFixtureGenTest.GeneratedSetDrivesThePipeline
[       OK ] CalibFixtureGenTest.GeneratedSetDrivesThePipeline (2536 ms)
[ RUN      ] CalibFixtureGenTest.ExpiryTimestampRoundTrips
[       OK ] CalibFixtureGenTest.ExpiryTimestampRoundTrips (5068 ms)
```

`GeneratedSetDrivesThePipeline` 은 산출 디렉터리를 `calibPath` 로 넘기고
`XPE_OK` · `XPE_FLAG_OFFSET_CORRECTED` · `XPE_FLAG_GAIN_CORRECTED` ·
`XPE_PIXEL_FLOAT32` 승격을 모두 확인한다.
`ExpiryTimestampRoundTrips` 는 과거 스탬프에서 `is_expired == true` /
`remaining_days < 0`, 미래 스탬프에서 그 반대를 확인한다.

### C6 — export 무증가

`g_calib` / `g_calib_mutex` 를 CLI 에서 참조하려 했을 때 LNK2001 로 링크가
깨졌다(측정됨). export 를 추가하는 대신 생성기가 쓴 파일을 `ifstream` 으로
직접 다시 읽는 경로로 바꿨다. `modules/preprocess/*` 의 `XPE_API` 선언은
이 카드에서 한 줄도 늘지 않았다 (`a36-diff.txt`: 헤더 파일 변경 0건).

### C7 — 전체 재측정 (`a36-ctest.log`)

명령: `ctest --output-on-failure` (build/ci-preprocess)

```
100% tests passed, 0 tests failed out of 571
Total Test time (real) =  51.77 sec
```

### 변경 규모 (`a36-diff.txt`)

```
 modules/preprocess/CMakeLists.txt                  |  45 ++
 modules/preprocess/tests/test_calib_fixture_gen.cpp| 216 ++++++
 modules/preprocess/tools/xpe_calib_fixture_gen.cpp | 452 +++++++++
 3 files changed, 713 insertions(+)
```

빌드 로그: `a36-build10.log` (error 0건, `/WX` 활성 상태에서 경고도 0건).

---

## 3. Baseline 귀속

- **테스트 총수**: QA-A-35 보고서가 기록한 566 건이 직전 baseline이다.
  이번 측정 571 건 = 566 + 신규 5건. 증감분이 이 카드가 추가한 케이스 수와
  정확히 일치하며, 기존 케이스의 상태 변화는 없다.
- **빌드 프리셋**: `ci-preprocess` 동일. `/WX` 는 QA-A-27 에서 실측으로
  활성 확인된 상태 그대로다.
- **RED 기준선**: RED 는 별도 트리가 아니라 **같은 트리에서 `created_epoch_ms`
  한 줄만 되돌린 뒤 재빌드**해 얻었다. 원복 후 같은 명령으로 GREEN 을 다시
  측정했으므로 두 값은 같은 트리·같은 명령 위에서 비교 가능하다.

---

## 4. 미검증 (Gaps)

1. **결정성은 이 머신·이 컴파일러에서만 측정됐다.** 다른 OS/컴파일러에서
   `std::mt19937` 은 규격상 동일하지만, 부동소수 축약(FMA 등)이 다르면
   gain 페이로드 바이트가 달라질 수 있다. 크로스 플랫폼 재현은 측정하지 않았다.
2. **1024×1024 기본값으로는 테스트하지 않았다.** 스위트는 256×256 으로 돈다
   (bright 검출기 하한 128 은 충족). 기본 크기 실행은 `a36-cli-run.log` 의
   수동 실행에도 없다 — 그 로그도 256/64 이다.
3. **manifest.json 의 SHA 값이 파일 내용과 일치하는지 테스트가 검사하지 않는다.**
   존재만 확인한다. 로그의 표준출력과 manifest 는 같은 계산 결과지만,
   회귀 테스트로 고정된 것은 아니다.
4. **카드 항목 — 카탈로그 매니페스트 경로 보고**:
   `gui/ImageProcTest/fixtures/gui-s0/calibration/` 갱신은 Lane C 소유라
   이 레인에서 건드리지 않았다. 경로만 리더에 보고한다.
5. **사용 문서**: `modules/preprocess/README` 는 존재하지 않는다. 사용법은
   `tools/xpe_calib_fixture_gen.cpp` 파일 헤더(10~40행)에 두었다.
   별도 문서 파일은 만들지 않았다(`docs/` 는 리더 소유).
6. **실행 시간 계측을 별도로 하지 않았다.** gtest 가 보고한 케이스별 시간
   (256×256 2회 생성 ≈ 5.1초)이 유일한 타이밍 증거다.

---

## 5. 잔여 위험 (Residual risk)

1. **`std::system` 의존.** 테스트가 도구를 서브프로세스로 띄운다. CI 가
   서브프로세스 생성을 제한하는 환경이면 5건 전부가 환경 사유로 깨진다.
   실패 메시지가 "exit code != 0" 뿐이라 원인 구분이 어렵다.
2. **경로 인용.** Windows 경로에 공백이 들어가면 `std::system` 인용 규칙이
   취약하다. 현재 빌드/임시 경로에는 공백이 없어 통과한다 — 다른 머신에서
   공백 경로면 깨질 수 있다.
3. **defect 파일의 시드 무관성을 테스트가 고정했다.** 앞으로 검출기 파라미터를
   바꾸면 `DifferentSeedChangesOffsetAndGain` 의 `defect 동일` 단언이
   의도치 않게 깨질 수 있다. 이는 설계 의도(주입 좌표 고정)를 지키는 잠금이므로
   깨지면 설계 변경 신호로 읽어야 한다.
4. **64×64 실행에서 defect 화소가 1272개**로 나온다(`a36-cli-run.log` EXPIRY).
   bright 창(128)보다 프레임이 작아 검출기가 과검출하는 상태로 보인다.
   도구는 거절하지 않고 그대로 통과시킨다 — 작은 프레임을 실사용 픽스처로
   쓰면 defect map 이 사실상 무의미해진다. 최소 크기 가드는 넣지 않았다
   (임계값을 발명하지 않기 위해). 필요하면 리더 판단으로 별도 카드.
