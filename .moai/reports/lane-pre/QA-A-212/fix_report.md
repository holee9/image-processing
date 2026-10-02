# QA-A-212b — XCal 쓰기의 목적지 교체를 잠깐 재시도한다 (수정 보고, Refs #233)

기준: 구현은 임시 커밋 `dc0d9d56` 에 있고, 이 보고서와 함께 마무리한다. 승인: 리더 결정(994cd53c 수신) — 5·32 만, 합계 약 100 ms, 읽기 전용도 상한 뒤 `IO_FAILED`, 최종 실패 알림에 오류·재시도 수, 성공 시 알림 없음, `replace_file` 을 지나는 호출 경로 전수. 푸시하지 않음.

## 변경 (`xcal_writer.cpp`)

- `replace_file` 이 Windows 에서 `MoveFileExA(REPLACE_EXISTING|WRITE_THROUGH)` 가 `ERROR_ACCESS_DENIED(5)` 또는 `ERROR_SHARING_VIOLATION(32)` 로 실패하면 **되풀이**한다. 쉬는 시간은 1 ms 에서 시작해 두 배씩 늘리다 25 ms 에서 멈추고, 쉰 시간의 합이 **`kReplaceRetryBudgetMs = 100`**(상수 하나)에 닿으면 멈춘다. 다른 오류는 즉시 최종 실패.
- 상수의 근거(주석에 같은 수치): 관찰된 모든 사건이 1 ms 안에 풀렸고(13/13), 100 ms 는 그 두 자릿수 위이면서 사람이 저장을 기다릴 때 눈치채지 못하는 쪽이다.
- 상한 뒤에도 실패하면 지금과 같이 임시 파일을 지우고 `XPE_ERR_IO_FAILED` 를 돌려주되, **알림**이 하나 붙는다. 성공하면(재시도 끝이라도) 알림 없음. 비 Windows(`std::rename`)는 바뀌지 않았고 알림도 없다. 형식·공개 함수·성공 경로 불변.

### 알림 전체 문구 (레인 간 계약, `XPE_ALERT_ERROR`)

```
XPE_WARN_XCAL_REPLACE_FAILED: could not replace '<path>' with the newly written calibration file: Windows error <GLE> after <N> retries (<reason>). The previous file, if any, is unchanged and the temporary file was removed
```

`<reason>` 은 GLE 가 5 또는 32 이면 `the destination stayed open in another process, is read-only, or may not be changed`, 그 밖이면 `not a transient condition, so it was not retried` (그때 `<N>` 은 0). `<path>` 는 호출자가 준 경로 그대로.

## 호출 경로 전수 (grep)

`replace_file` 은 `write_xcal_file_ex` 안에서만 불리고(`xcal_writer.cpp`), 다른 `MoveFile`/`ReplaceFile`/`rename` 호출은 `modules/` 제품 코드에 없다(`grep -rnE "MoveFile|ReplaceFile|std::rename|rename\(" modules`: 제품 소스는 `xcal_writer.cpp` 한 곳; `std::remove` 는 임시 파일·도구 정리용). `write_xcal_file` 을 부르는 제품 코드(시험·도구 제외):

| 호출자 | 위치 |
|---|---|
| `xpe_calib_generate_gain` / `xpe_calib_generate_gain_polynomial` | `xpe_calib_generate_gain.cpp:402`, `:885` |
| `xpe_calib_generate_nonlin_lut` | `xpe_calib_generate_nonlin_lut.cpp:348` |
| `xpe_calib_generate_offset` | `xpe_calib_generate_offset.cpp:162` |
| `xpe_calib_save` | `xpe_calib_save.cpp:107` |
| (도구) `xpe_calib_fixture_gen` | `tools/xpe_calib_fixture_gen.cpp:231` — 제품이 아니라 시험 픽스처 생성기 |

모두 같은 `replace_file` 을 지난다. 놓친 직접 이동 호출은 없다. 시험은 이 중 `write_xcal_file` 직접과 공개 `xpe_calib_save` 를 끝까지 가둔다(생성기 세 개는 같은 한 줄 호출이라 별도 시험 없이 grep 으로만 확인 — Gaps).

## 시험 (`test_xcal_replace_retry.cpp`, Windows 전용 5건, 다른 OS 는 건너뜀)

"다른 프로세스"는 시험 안의 스레드가 목적지를 `FILE_SHARE_DELETE` 없이 읽기로 열고 정해진 시간 붙잡는 것이다(QA-A-212 가 별도 프로그램으로 재현한 메커니즘과 같다).

| 시험 | 단언 |
|---|---|
| 대조 | 붙잡힌 목적지에 맨 `MoveFileEx` 는 실패하고 오류가 정확히 5 — 점유가 실제로 무는지 |
| 상한 안(30 ms 점유) | 쓰기 성공, 새 파일 내용, 임시 파일 없음, **알림 0** |
| 상한 밖(700 ms 점유) | `IO_FAILED`, 걸린 시간 < 5000 ms(상한만), 옛 파일 그대로, 임시 파일 없음, 알림에 `Windows error 5`·경로·재시도 수 > 0 |
| 읽기 전용 목적지 | `IO_FAILED`(상한 뒤), 걸린 시간 < 5000 ms, 임시 파일 삭제, 옛 파일 보존, 알림에 `Windows error 5` |
| 공개 `xpe_calib_save` | 30 ms 점유엔 성공(알림 0), 700 ms 점유엔 `IO_FAILED`+임시 없음+알림 |

시간 단언은 **상한만**이다(하한 없음).

### 반증 (한 번에 하나, 전체 빌드, `evidence/40_arm_c*.txt`)

| 손상 | 빨강이 된 시험 |
|---|---|
| c1 재시도 없음 | 3건(상한 안·상한 밖·공개 save) |
| c3 상한 8000 ms | 3건(상한 밖·읽기 전용·공개 save) |
| c4 실패 알림 제거 | 3건 |
| c5 재시도 뒤 성공에도 알림 | 2건 |
| c6 실패 시 임시 파일 안 지움 | 3건 |
| c7 재시도 횟수를 세지 않음 | 1건 |
| c8 32 만 재시도 | 3건 |
| **c2 모든 오류를 재시도** | **0건 — 시험이 못 가둔다** |

c2 가 안 터지는 이유와 한계: "5·32 가 아닌 오류는 즉시 실패" 를 시험하려면 `MoveFileEx` 가 다른 코드로 실패하게 만들어야 하는데, 시험이 안정적으로 만들 수 있는 실패(읽기 전용·점유)는 전부 5·32 다. 이 조건은 코드와 리뷰로만 지켜진다. 안 터진 반증이 곧 중복이라는 뜻은 아니다(그 방어만 걸리는 입력을 못 만들었을 뿐) — 지우지 않았다.

## 검증

211b 의 전체 검증(`evidence/` 가 아니라 `.moai/reports/lane-pre/QA-A-211b/evidence/verify/`)이 이 구현이 들어 있는 상태에서 돌았다: preprocess 854 통과(셔플 포함 854), oom 60, common 69·12, ctest 1044, export diff 0, 헤더 문서 0건. 임시 진단 수정(QA-A-212)은 되돌려져 있음을 `git diff` 로 확인했다.

## 반경 16 의 최악 시간 (리더 요청, `evidence/50_…py`, `51_…new.txt`, `52_…old_dlls.txt`)

3072×3072, `xpe_defect_correct` 단독, float32 프레임, 3회 중 최소(ms). "이전" 은 QA-A-210d 빌드 DLL(덩어리 안쪽을 0 으로 씀, 탐색 없음).

| 마스크 | 마스크 화소 | 이전 | 이후 | 이후의 화소 결과 |
|---|---|---|---|---|
| 마스크 없음(조기 반환) | 0 | 4.3 | 4.1 | — |
| 단독 2 + 쌍 1 | 3 | 7.5 | 9.9 | — |
| **큰 덩어리 하나 686×686 (4.99%, 깊은 안쪽이 반경 16 밖)** | 470,596 | **19.9** | **638.5** | 반경 16 밖이라 입력값 유지 427,716, 0 은 0 개 |
| 20×20 덩어리 격자(11.0%, 안쪽 거리 ≤ 10) | 1,040,400 | 54.3 | 351.7 | 전부 채움 |
| 40×40 덩어리 격자(19.6%, 안쪽 거리 최대 20) | 1,849,600 | 66.0 | 1,217.6 | 입력값 유지 73,984 |
| **CalData_6 `BPMap.map` (실제 결함 맵)** | 23,505 | 9.0 | **11.9** | 전부 채움, 0 개 |

읽는 법: 실제 결함 맵은 +2.9 ms(9.0 → 11.9)로 무시할 만하다. **병적인 경우 — 반경 16 까지 정상 화소가 없는 큰 덩어리가 상한 근처(5%)로 있는 프레임 — 은 약 640 ms**이고, 마스크 화소마다 반경 16 의 1,089 칸을 모두 보는 비용이 그대로 나온다(470 천 × 약 1.1 천). 이는 SPEC 의 전체 파이프라인 목표(< 500 ms)를 이 단계 하나로 넘는다. 20% 격자는 1.2 s. 이런 프레임은 어차피 결함 밀도가 SRS 의 5% 를 넘거나 덩어리가 검출기 결함의 모양이 아니다(합집합 5% 초과는 경고).

**개선 여지(구현하지 않았다 — 요청은 측정)**: 한 화소가 반경 16 에서 못 찾았으면, 그 화소의 이웃 중 같은 덩어리 안쪽에 있는 화소들도 대부분 못 찾으므로 덩어리 단위로 "가장 가까운 정상 화소까지 거리" 를 한 번(경계에서 안쪽으로 번져 나가는 BFS, O(마스크 화소))에 구하면 화소별 고리 탐색이 필요 없다. 결과 값은 같게 유지할 수 있다. 시간이 문제가 되면 별도 카드.

## 미검증 (Gaps)

- 5·32 이외 오류가 재시도되지 않는다는 것의 시험(위 c2).
- 생성기 세 개(gain, nonlin LUT, offset)를 끝까지 부르는 시험은 없다 — 같은 한 줄 `write_xcal_file` 호출임을 grep 으로 확인했을 뿐이다.
- 실제 백신·색인기와의 상호작용(점유 스레드는 흉내 낸 것).
- 재시도 중 쉬는 시간이 호출 스레드를 잠그는 영향(최악 약 100 ms 지연)은 측정하지 않았다.
- (211b 의 반경 16 시간은 위 절에서 측정함. 파이프라인 전체 시간 목표와의 관계는 단계 단독 시간으로만 비교했다.)

## 잔여 위험

- 목적지를 100 ms 넘게 붙잡는 프로세스가 있으면 이전과 같이 실패한다 — 이제는 알림이 이유를 말한다.
- `ERROR_ACCESS_DENIED` 를 내는 영구 원인(권한·읽기 전용)은 매번 100 ms 를 쓰고 실패한다.
