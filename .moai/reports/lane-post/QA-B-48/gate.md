# QA-B-48 게이트 보고서 — 실패한 읽기가 호출자에게 남기는 것

**카드**: QA-B-48 (#120, B-47 Residual 2번째 — "실패한다는 봤지만 올바르게 실패하는지는 아니다")
**레인**: Lane B (`xpe-post`, `dev/postprocess`)
**증거 경로**: `.moai/reports/lane-post/QA-B-48/`
**커밋 1건**: `709f117` — **제품 코드 변경 0**

---

## 1. 주장 (Claim)

| # | 주장 | 상태 |
|---|------|------|
| C1 | J2K 실패 4경로는 `outImg` 를 **전혀 건드리지 않는다** | PASS (관측 → 단언) |
| C2 | 제품 코드는 이미 옳아서 **고치지 않았다** | PASS |
| C3 | `:351-353` 은 라벨 교체 입력으로 **도달 불가**다 — B-29 의 물음이 규명됐다 | PASS |
| C4 | 짧은 PixelData 가 `XPE_OK` + 0 패딩으로 성공한다 — **기록이지 승인이 아니다** | **리더 결정 필요** |
| C5 | 불변 게이트의 민감도를 반증으로 증명했다 (빌드 성공 상태에서 실패) | PASS |
| C6 | 재실측 **465 / 209 / 167**, 경고 0 | PASS |

---

## 2. 관측을 먼저 했다 (`_observation.log`)

단언을 쓰기 전에 값을 찍었다. 센티널은 호출자가 절대 만들지 않을 값이라,
구현이 건드린 필드가 그대로 드러난다.

```
garbage     rc=-3  data=null w=4242 h=2424 dataSize=777 bitsAlloc=99
truncated   rc=-3  data=null w=4242 h=2424 dataSize=777 bitsAlloc=99
emptyfrag   rc=-13 data=null w=4242 h=2424 dataSize=777 bitsAlloc=99
tableonly   rc=-13 data=null w=4242 h=2424 dataSize=777 bitsAlloc=99
shortpixels rc=0   data=NON-NULL w=256 h=256 dataSize=131072 bitsAlloc=16
shortpixels tail: 0 non-zero of 32768
```

**네 경로 모두 센티널이 살아 있다.** 디코드 실패는 전부 `xpe_alloc_image` 이전에
일어나므로 남길 것 자체가 없다. 계약은 "비워서 돌려준다" 보다 강한
**"아예 쓰지 않는다"** 였고, 코드가 이미 그렇게 하고 있다.

따라서 **제품 코드를 고치지 않았다.** 카드의 지시 그대로다 — 같으면 단언으로 고정만 한다.

---

## 3. `:351-353` — B-29 가 남긴 물음의 규명 (`_351.log`)

B-29 는 "J2K 선언 + 네이티브 픽셀" 케이스에 실패를 기대하는 단언을 썼다가
`XPE_OK` 를 보고 **거짓 기대를 지웠다**. B-47 은 추측하지 않고 넘겼다.

이번에 실제로 돌려 봤다:

```
j2k-labelled-native open rc=-13
```

메타의 TS 만 `.4.90` 으로 바꾼 파일은 **`xpe_dicom_open` 에서 DICOM_INVALID 로
거절된다.** DCMTK 가 "캡슐화 선언 + 네이티브 픽셀" 데이터셋을 파싱하지 않으므로
`readImage` 에는 닿지도 않는다. **B-29 의 `XPE_OK` 는 재현되지 않았다.**

**커버리지 귀결**: `DicomReader.cpp:351-353`(캡슐화 표현 부재)은 이 입력으로는
**도달 불가**다. 추측이 아니라 **사유가 붙은 미도달**로 분류해 둔다.

---

## 4. 성공 경로에서 나온 소견 — 짧은 PixelData

실패 조사 중에 나온 것이라 카드 범위 밖이지만, 그대로 두면 아무도 다시 보지 않는다.

| 관측 | 값 |
|---|---|
| 선언 크기 | 256 × 256 (Rows/Columns) |
| 실제 PixelData | 선언의 **절반** |
| 반환값 | **`XPE_OK`** |
| 반환 버퍼 | 전체 크기(131072 B), 꼬리 32768 픽셀 **전부 0** |

`DicomReader.cpp:204-208` 이 `availBytes` 만 복사하고 `XPE_OK` 를 돌려준다.
꼬리가 0인 것은 `xpe_alloc_image` 가 memset 하기 때문(`xpe_memory.cpp:51`)이라
**미초기화 힙은 아니다.** 대신 판단하기 더 곤란한 것을 준다 —
**"성공적으로 읽었다"고 보고된 전체 크기 영상의 절반이 검정 패딩이고,
반환 코드는 그 사실을 말하지 않는다.**

**이것을 고치지 않았다.** SPEC·헤더 어디에도 "짧은 PixelData 는 성공이다/실패다"
문장이 없다. B-43 판별 기준으로는 **근거 문장이 없는 동작**이므로 기록이고,
에러로 바꾸는 것은 동작 변경이라 이 카드 범위 밖이다.
`KnownDivergence_` 접두어로 오늘의 동작만 고정했다 — 결정이 내려질 때
조용한 표류가 아니라 **변경**으로 보이도록.

> **리더 결정 요청**: 짧은 PixelData 를 (a) 현행 유지(XPE_OK + 0 패딩),
> (b) `XPE_ERR_DICOM_INVALID`, (c) XPE_OK 이되 계약을 헤더에 명문화 — 중 하나.

---

## 5. 반증 — 게이트가 민감하다 (`_falsify.log`)

`readImage` 가 분기 전에 `outImg->width` 를 쓰도록 약화시켰다
(실제 리팩터링에서 충분히 나올 수 있는 형태 — 치수를 미리 채워 두기):

```
===BUILD=0===                    <- 빌드 성공. 낡은 바이너리가 아니다
garbage bitstream: width was overwritten
truncated bitstream: width was overwritten
empty fragment: width was overwritten
offset table only: width was overwritten
[  FAILED  ] DicomJ2kFailureTest.FailedReadLeavesOutputUntouched
===EXIT=1===
```

**4경로 전부 잡았다.** B-46 처럼 재현되지 않는 결과가 아니라, 게이트가 실제로
그 종류의 회귀를 잡는다는 것이 확인됐다. 반증 뒤 제품 코드는 원복했고
`git diff --stat` 이 테스트 파일 하나만 보여 준다.

---

## 6. Baseline 귀속 (Baseline-attribution)

| 항목 | 값 | 근거 |
|---|---|---|
| 관측 | 4경로 센티널 전부 생존 | `_observation.log` |
| `:351-353` | open 단계 rc=-13 | `_351.log` |
| 신규 3케이스 | BUILD=0, 3/3 통과 | `_run1.log` |
| 반증 | BUILD=0, 4경로 전부 실패 = **재현됨** | `_falsify.log` |
| 이전 ctest | 465 / 209 / 164 | QA-B-47 `_verify.log` |
| 현재 ctest | **465 / 209 / 167** (dicom +3) | `_verify.log` |
| 빌드 경고 | 0 (`grep -c "warning C"`) | `_verify.log` |
| 제품 코드 변경 | **0** | `git diff --stat` = 테스트 파일 하나 |

---

## 7. 미검증 (Gaps)

- **JPEG-LL 실패 경로의 출력 상태는 보지 않았다.** `chooseRepresentation` 실패
  (`:174-178`)도 할당 전에 반환하므로 같을 것으로 **판독**되지만, 측정하지 않았다.
- **`xpe_free_image` 뒤의 구조체 상태를 단언하지 않았다.** PixelData 부재 분기
  (`:199-202`)는 할당 뒤에 실패하므로 "건드리지 않는다" 가 성립할 수 없다 —
  다른 계약이고, 이 카드가 다루지 않았다.
- **커버리지는 측정하지 않았다.** x64 OpenCppCoverage 는 설치하지 않기로 한 결정이
  유효하고, 수치는 CI dispatch 가 준다.
- **opj 생성 실패 분기**(`:388-392`, `:412-417`)는 여전히 미도달이다 —
  "도달 불가" 가 아니라 **"현재 도구로 도달 비용 과도"** 분류 그대로(폴트 주입 미도입).
- **Writer 의 openjpeg 인코더 실패 27줄은 손대지 않았다.**

---

## 8. 잔여 위험 (Residual-risk)

- **불변 계약이 문서에 없다.** 테스트가 유일한 기록이라, 헤더만 읽는 사람은
  실패 시 `outImg` 를 어떻게 다뤄야 할지 알 수 없다. 계약 문장을 헤더에 넣는 것은
  Lane B 소유 밖(api-spec)이라 제안만 남긴다.
- **짧은 PixelData 는 결정이 나기 전까지 실제 위험이다.** 현장에서 잘린 파일이
  오면 반쪽짜리 영상이 "정상"으로 표시된다. 단언은 표류를 막을 뿐 위험을 없애지 않는다.
- **`J2kLabelledNativePixels_RejectedAtOpen` 은 DCMTK 동작에 묶여 있다.**
  DCMTK 가 관대해지는 방향으로 바뀌면 이 단언이 먼저 깨지는데, 그때 깨지는 것이
  오히려 옳다 — `:351-353` 의 분류가 그 순간 무효가 되기 때문이다.
- 커밋은 push 전까지 미푸시 유일본이다.

---

## 부록 — 증거 파일

| 파일 | 내용 |
|---|---|
| `_env.bat` / `_verify.bat` | 환경 + 세 프리셋 재빌드·전체 ctest |
| `_obs.bat` / `_observation.log` | 관측 패스 — 실패 뒤 outImg 상태 |
| `_351.log` | J2K 라벨 교체 네이티브 파일 → open rc=-13 |
| `_b48.bat` / `_run1.log` | 신규 3케이스 BUILD=0, 3/3 |
| `_falsify.log` | 이른 `outImg->width` 쓰기 — BUILD=0, 4경로 전부 실패(**재현됨**) |
| `_verify.log` | 최종 465 / 209 / 167, 경고 0 |
