# QA-B-216 — 2단계 기준 영상을 출하 CI DLL 로 다시 만든다 (m2-post-v3) + 컴파일러 변화 감지 설계 (#251)

제품 코드 변경 없음. 바뀐 것: `display_api.h` 주석의 이슈 번호(#151 → #259, 리더 요청), 측정 하니스 시험 하나(`DISABLED_FingerprintFixture_216`).

## 결론

| 항목 | 결과 |
|---|---|
| origin/main 병합 | `f032eef4`. 충돌 2건(둘 다 add/add)을 내 쪽으로 풀었다(아래 0절). 푸시 안 함 |
| CI DLL | main push 실행 **38217501792**(커밋 `de5afc01`)의 `post-build` 잡, artifact `xpe-ci-post-binaries`. MSVC **19.51.36260.0**(VCToolsVersion 14.51.36231) |
| m2-post-v3 | `xpe-data/m2-post-v3/` 8장 + `README.txt` + `provenance.txt` + `sha256.txt` |
| 같은 DLL 반복 | 전체 실행을 두 번 돌려 원시 출력 24개가 **바이트 동일** |
| v2 대 v3 | 00·01 동일, 02~07 은 0.06%~9.2% 화소가 다름. 최종 영상(05) 6137개, 최대 차 384 — QA-B-215 가 잰 값과 같다 |
| 감지 설계 | 버전 기록 비교(A) + 합성 지문 시험(B) 초안. B 가 14.44 대 14.51 을 가른다는 것을 실측 |

## 0. 병합

`git merge --no-ff origin/main`(386182e1, rebase 아님). 충돌 2건:

| 파일 | 내용 | 해결 |
|---|---|---|
| `tests/e2e_post_pipeline/test_m2_measure.cpp` | 내가 이 브랜치에서 추가한 `StageDump_215`(52줄)와 main 쪽 사본이 같은 위치에서 add/add | 내 쪽(HEAD) 유지. main 쪽 블록은 비어 있었다 |
| `.moai/reports/lane-post/QA-B-210/report_e7.md` | 상한 근거 문구. 이 브랜치의 8607a074 가 고친 문구와 main 의 옛 문구 | 내 쪽(고친 문구) 유지 |

병합 뒤 `modules/display`·`modules/enhance_basic` 트리가 `origin/main` 과 같다(`git diff origin/main HEAD -- modules/display modules/enhance_basic` 비어 있음). 이후 `origin/main...HEAD` 는 `0 35`.

## 1. CI DLL 확보

| 항목 | 값 |
|---|---|
| 실행 | `38217501792`, workflow "XPE CI Pipeline", 이벤트 push, 브랜치 main |
| 커밋 | `de5afc01d10a5d118de9d135e33c2c1391c48c63` |
| 왜 386182e1 이 아닌가 | 386182e1 의 CI 실행(38216547608)은 de5afc01 push 로 **취소**됐다(cancel-in-progress). 리더 확인: de5afc01 은 요구 문서만 바뀌었다. `modules/enhance_basic`·`modules/display` 트리 해시가 de5afc01 과 HEAD 에서 같다(`a641739…`, `2ba518b…`) |
| 잡 / artifact | `post-build`(잡 114705792565) 의 "Upload post binaries" → `xpe-ci-post-binaries`(artifact 11733236436) |
| 컴파일러 | 잡 로그 `VCToolsVersion: 14.51.36231`, `The CXX compiler identification is MSVC 19.51.36260.0` (`ci_job_toolchain_lines.txt`) |
| sha256 | `dll_sha256_ci_de5afc01.txt` (enhance_basic `ac0a09b9…`, display `874293ad…`, common `e8047f05…`, spdlog `3e93147e…`, fmt `7c6a03a0…`) |

보충: 이 artifact 로 만든 최종 uint16 은 39f9727a 때의 CI DLL(QA-B-215 에서 쓴 것)로 만든 것과 **차이 0**이다. 서로 다른 두 CI 실행이 같은 컴파일러(14.51.36231)에서 같은 수치를 낸다.

## 2. m2-post-v3 생성

m2-post-v2 와 같은 하니스(`M2Measure.DISABLED_Chain_P1_P3_P4_P5_P7` → `DISABLED_Variants_P8`)·입력(`m1-baseline-v2/…f32le.raw`, sha256 `d152e8eb…fdfd4`)·단계 목록·PNG 스크립트(`make_pngs_v3.py.txt`, v2 스크립트와 같은 본문). 달라진 것은 하니스 실행 파일 옆에 놓은 DLL 뿐(`run216.py.txt`). 실행 파일은 로컬 `ci-post` 빌드이고 DLL 의 C API 만 부른다. 자동 창: 중심 2580.3467, 폭 1198.4963 (v2: 1198.4956).

## 3. v2 대 v3 (`compare_v2_v3_repeat.txt`, 3절 c)

| 파일 | 다른 화소 | 최대 차 |
|---|---|---|
| 00 input | 0 | 0 |
| 01 log | 0 | 0 |
| 02 noise_bilateral | 126,489 (1.34%) | 1 |
| 03 clahe | 869,940 (9.22%) | 76 |
| 04 usm | 800,596 (8.48%) | 337 |
| **05 FINAL** | **6,137 (0.065%)** | **384** |
| 06 ALT | 5,561 (0.059%) | 385 |
| 07 OLD | 16,801 (0.178%) | 321 |

(02~04 는 각 영상이 자기 p1~p99 창으로 그려진 표시용 16비트라 차이 크기가 float 차이와 직접 비례하지 않는다.)

QA-B-215 와 맞는지: 05 의 6137개·최대 384 는 QA-B-215 가 모듈 단계 덤프로 잰 값과 정확히 같다. 대조군: 같은 하니스를 post-local DLL 로 다시 돌린 출력은 **공개된 v2 PNG 와 8장 모두 차이 0**(3절 b)이라, 병합이 v2 의 수치를 건드리지 않았고 위 차이는 DLL 때문이다.

### v2 를 인용하는 곳 (리더가 v3 로 바꿀 목록)

`git grep -n "m2-post-v2"` (HEAD, QA-B-213 하위·이 카드 증거 제외):

| 위치 | 성격 | 처리 제안 |
|---|---|---|
| `.moai/project/milestone-3-operator-app.md:14` (U4 행) | **기준 영상 지정** — 앱이 그린 영상과 비교할 대상 | v3 `05_FINAL…` 로 교체. 비교는 같은 DLL(CI) 로 앱을 돌린 뒤 |
| `.moai/reports/lane-gui/GUI-C-233/report.md:30` | 233 시점 보고서의 사실 기록 | 소급 수정하지 않는 것을 권한다(그때 쓴 DLL 의 기록) |
| `.moai/reports/lane-post/QA-B-214/report.md:13,83,107` | 214 증거 | 그대로(역사) |
| `.moai/reports/lane-post/QA-B-214b/report.md:83`, `QA-B-214c/report.md:17,88` | 214b·c 증거 | 그대로 |
| `.moai/reports/lane-post/QA-B-215/report.md`, `draft_REQ-ENH_determinism_v1.txt` | 215 의 처방이 v2 를 언급 | 그대로. 215 의 처방이 이 카드로 이행됨 |
| `xpe-data/m2-post-v2/README.txt` | v2 자체 | 그대로 두되 "post-local MSVC 14.44 DLL 로 만듦, 출하 기준은 v3" 한 줄 추가를 제안 |

검색 범위: 이 저장소의 HEAD 에 커밋된 파일. `xpe-gui` 워크트리의 미커밋 파일과 다른 저장소는 보지 않았다.

## 4. 같은 DLL 반복

`run216.py.txt ci ci1`, `… ci ci2` 를 같은 DLL 로 두 번. 출력 폴더의 원시 파일 24개(단계 float·uint16·변형 8종·마스크 등, 타이밍·메모리 csv 제외) 모두 sha256 동일(`differing: 0`). 같은 DLL 결정성의 직접 증거다.

## 5. 컴파일러 변화 감지 설계

설계는 `compiler_change_detection_design.md`, 패치 초안은 `patch_draft_1`~`3`(`.txt`, 적용하지 않음). 핵심:

- A 버전 기록 비교(`tools/ci/Test-ToolchainRecord.ps1`, `toolchain-record.txt`), B 합성 지문 시험(`modules/enhance_basic/tests`).
- B 가 쓸 만한지 측정: 정수로만 만든 512² 합성 입력 3개에서 양방향 해시가 MSVC 14.29 = 14.44, **14.51 은 다름**, `/fp:precise` 도 다름(`fixture_*.txt`). 수 ms 짜리 입력으로 14.44→14.51 변화를 잡는다.
- 경고 대 실패: 장단점 표를 적고, 권고는 "버전 비교는 main push 에서만 실패, PR 에서는 경고. 지문 시험은 표에 있는 도구 모음에서만 실패, 없으면 skip 하며 해시를 출력".

## 6. 주석 수정 (리더 요청)

`modules/display/include/xpe/display/display_api.h` 의 `#151` 6곳을 `#259` 로(XpeBodyPart 값 주석 4, 줄 95·253 설명). 코드 변경 없음, doxygen stderr 0바이트(`doxygen_err.txt`). 같은 모듈의 `voi_lut.cpp`·시험 파일 주석에도 `#151` 이 남아 있다(요청 범위 밖이라 두었다).

## 5절 요약 (verification-claim-integrity §3)

**Claim**: 위 결론 표. **Evidence**: 이 폴더의 `chain_*.txt`, `compare_v2_v3_repeat.txt`, `dll_sha256_ci_de5afc01.txt`, `ci_job_toolchain_lines.txt`, `fixture_*.txt`, `doxygen_err.txt`, 설계·패치 초안.
**Baseline-attribution**: CI 실행 38217501792(de5afc01) artifact + 같은 하니스 실행 파일(로컬 ci-post 빌드, dev/postprocess `f032eef4` 위 작업 트리). 대조군은 같은 실행 파일을 post-local DLL 로 돌린 출력(v2 PNG 와 동일).
**Gaps (미검증)**:
- 앱(WPF)이 이 DLL 로 그린 영상과 v3 05 의 비교는 하지 않았다(GUI-C-234 의 일).
- 설계 초안의 PowerShell·워크플로 조각은 러너에서 돌려 보지 않았다. 지문 시험은 스켈레톤이고 컴파일하지 않았다. `_MSC_VER` 값(1944/1951)은 러너에서 확인해야 한다.
- 지문 3개 입력이 EI 경로 등 모든 코드 경로를 지나지는 않는다.
- v2 인용 목록은 HEAD 의 커밋된 파일만 대상으로 했다.
- 하니스 실행 파일은 로컬 컴파일러(14.44)로 만들었다. DLL 이 낸 수치가 실행 파일 쪽 컴파일에 의존하지 않는다는 것은 C API 경계(float 버퍼)라는 구조에 기댄다 — 대조군(post-local 재실행이 v2 와 동일)이 이를 뒷받침하지만 CI 실행 파일로 따로 돌려 보지는 않았다.
**Residual-risk**: 러너 이미지가 갱신되면 다음 CI 산출물은 또 달라질 수 있다(고정 없음, 5절 설계가 이를 다룬다). v3 는 14.51.36231 에 대한 기준이다.
