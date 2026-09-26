# QA-B-51 게이트 보고서 — 선언보다 큰 산출도 거절한다

**카드**: QA-B-51 (#150 #120)
**레인**: Lane B (`xpe-post`, `dev/postprocess`)
**증거 경로**: `.moai/reports/lane-post/QA-B-51/`
**커밋 1건**: `5f35268`
**선행**: `git merge origin/main` 완료 (B-50 병합분 `3b6b139` 포함)

---

## 1. 현행 관측 — 하나의 형태에 두 개의 답

변경 전(`_observation.log`, **BUILD=0**). B-50 이 `KnownDivergence_` 로 고정해 둔
현행이 그대로 통과하는 것으로 재확인했다.

| 케이스 | 선언 | 실제 코드스트림 | 반환 코드 | 반환 크기 |
|---|---|---|---|---|
| `j2k-oversized` | 256×**128** | 256×**256** | **0 (XPE_OK)** | **256×256** — 선언보다 큰 버퍼 |
| `jpegll-oversized` | 256×**128** | 256×**256** | **-3 (PROCESSING_FAILED)** | — |
| `j2k-undersized` (B-50) | 256×256 | 256×128 | -13 | 미기록(거절) |
| `jpegll-undersized` (B-50) | 256×256 | 256×128 | -13 | 미기록(거절) |
| **대조군** `CompressedMatchedSize` (J2K·JPEG-LL) | 256×128 | 256×128 | **0** | 256×128 |
| **비압축 여분 바이트** `SurplusPixelData` | — | 선언 + 64px | **0** | 정상 |

**같은 잘못된 형태에 경로마다 다른 답이 있었다.** 그것 자체가 계약이 정해지지 않았다는
가장 분명한 증거였다. 대조군이 같은 제작 방식으로 정상 판독되므로, 위의 차이는 픽스처가
아니라 **불일치의 처리 방식** 때문이다.

---

## 2. 변경 (`5f35268`)

두 가드를 `<` 에서 `!=` 로 넓혔다.

| 지점 | 이전 | 이후 |
|---|---|---|
| `decodeJ2KBitstream` | `imgW < cols \|\| imgH < rows` | `imgW != cols \|\| imgH != rows` |
| `readImage` JPEG-LL 분기 (SOF 대조, `chooseRepresentation` 이전) | `frameW < cols \|\| frameH < rows` | `frameW != cols \|\| frameH != rows` |

로그 문구도 "smaller than declared" 에서 "does not match the declared size" 로 바꾸고,
**선언 치수와 실제 치수를 둘 다** 남기는 것은 그대로 유지했다.

**B-50 이 세운 구분은 유지된다**(카드 2항). OpenJPEG 가 풀지 못한 경우는 계속
`XPE_ERR_PROCESSING_FAILED` — "디코드하지 못했다" 이다. 이번 판정은 "풀었는데 약속과
다르다" 이므로 `XPE_ERR_DICOM_INVALID` 다.

여기에 이번 카드가 더한 것이 하나 있다. **JPEG-LL 의 큰 쪽은 전에도 실패하고 있었지만,
디코더 실패(`PROCESSING_FAILED`)로 드러났다** — 그것은 파일의 결함을 **잘못된 이름으로
부르는 것**이었다. 디코더가 고장난 게 아니라 파일이 자기 서술과 어긋난 것이다.
SOF 대조가 `chooseRepresentation` 보다 앞서므로, 이제 같은 파일이 두 경로에서 **같은
코드로, 실제 결함의 이름으로** 거절된다.

**출력 버퍼**: 두 판정 모두 `xpe_alloc_image` 이전이라 B-48 계약이 온전히 성립한다
(센티널 `4242/2424` 생존을 단언).

---

## 3. 넘지 않은 경계 — 한 줄

**비압축의 여분 바이트는 계속 성공이다(B-49 판정 유지). 여분 바이트는 DICOM 의 흔한
패딩이라 치수 주장과 모순되지 않고, 여분 치수는 치수 주장 자체와 모순된다.**

`SurplusPixelData_IsIgnoredAndReadSucceeds` 가 RED·GREEN·반증·최종 전 구간에서
**한 번도 흔들리지 않고 통과**했다(각 로그의 해당 줄). 이번 변경이 그 경계를 넘지 않았다는
증거다.

---

## 4. RED → GREEN, 그리고 뒤집힌 단언

| 단계 | 빌드 | 결과 | 로그 |
|---|---|---|---|
| RED | **BUILD=0** | 큰 쪽 2건 실패 — J2K `rc=0`·반환 256(기대 4242 센티널), JPEG-LL `rc=-3` | `_red.log` |
| GREEN | **BUILD=0** | **7/7** 통과 | `_green.log` |

**B-50 의 `KnownDivergence_CompressedLargerThanDeclared` 는 틀렸던 게 아니라 대체됐다.**
계약이 없던 동안 추측 대신 현행을 붙들고 있던 기록이고, 계약이 생기자 기대가 확정됐다.
경위 — 무엇을 대체했는지, 무엇이 그것을 대체하게 했는지(경로별로 답이 갈린 것 자체가
미결의 증거였다는 판독), 왜 "정정" 이 아니라 "대체" 인지 — 를 주석에 남기고 정상 기대
2건(`J2kCodestreamLargerThanDeclared_…`, `JpegLosslessFrameLargerThanDeclared_…`)으로
나눴다(B-41·B-42·B-49 방식).

---

## 5. 반증 — 새로 넓힌 절반이 실제로 일한다 (`_falsify.log`)

가드를 지우지 않고 **`<` 로 되돌려 약화**시켰다(= 이번 변경 이전 상태):

```
===BUILD=0===                    <- 빌드 성공
[       OK ] ShortPixelData_ReturnsDicomInvalid
[       OK ] SurplusPixelData_IsIgnoredAndReadSucceeds
[       OK ] J2kCodestreamSmallerThanDeclared_ReturnsDicomInvalid
[       OK ] JpegLosslessFrameSmallerThanDeclared_ReturnsDicomInvalid
[       OK ] CompressedMatchedSize_StillReadsNormally
[  FAILED  ] J2kCodestreamLargerThanDeclared_ReturnsDicomInvalid
[  FAILED  ] JpegLosslessFrameLargerThanDeclared_ReturnsDicomInvalid
===EXIT=1===
```

**큰 쪽 2건만 실패하고 나머지 5건은 통과한다.** 이번에 넓힌 절반이 정확히 그 두 경우만
잡고 있고, 작은 쪽·대조군·비압축에는 **영향을 주지 않았다**는 것이 같은 로그 한 장에 있다.
약화 형태를 고른 이유는 B-49·B-50 과 같다(조건 삭제 → 변수 미사용 → `/WX` 파손 →
낡은 바이너리가 통과를 찍음). 반증 뒤 제품 코드는 원복했다.

---

## 6. Baseline 귀속 (Baseline-attribution)

| 항목 | 값 | 근거 |
|---|---|---|
| 변경 전 관측 | J2K `rc=0`(256×256 반환) / JPEG-LL `rc=-3`, 대조군 정상 | `_observation.log` (BUILD=0) |
| RED | BUILD=0, 큰 쪽 2건 실패 | `_red.log` |
| GREEN | BUILD=0, 7/7 | `_green.log` |
| 반증 | BUILD=0, **큰 쪽 2건만** 재실패 = **재현됨** | `_falsify.log` |
| 이전 ctest | 465 / 209 / 172 | QA-B-50 `_verify.log` |
| 현재 ctest | **465 / 209 / 173** (KnownDivergence 1건 → 정상 기대 2건) | `_verify.log` |
| 빌드 경고 | 0 (`grep -c "warning C"`) | `_verify.log` |
| 변경 파일 | 3 (`DicomReader.cpp`, `dicom_api.h`, `test_dicom_reader.cpp`) | `git diff --stat` |

**이 변경으로 깨진 기존 단언은 없다** — 847건 중 실패 0.

---

## 7. 미검증 (Gaps)

- **`Columns` 쪽 불일치는 시험하지 않았다.** 픽스처가 `Rows` 만 조작한다. 가드는 두 축을
  대칭으로 비교하므로 같은 결과가 **판독**되지만, 측정하지 않았다.
- **SOF 파서는 여전히 단일 프레임 전제다**(B-50 Gap 그대로). 다중 프레임에서는 item 1 만 본다.
- **SOF 를 못 읽는 JPEG 는 무판정**이며, 그런 파일의 실재 여부를 시험하지 않았다.
  `!=` 로 넓어진 뒤에도 무판정은 **거절이 아니라 통과**이므로 위험 방향은 안전 쪽이다.
- **`comps[0]` 만 사용한다** — 컬러·다중 컴포넌트는 보지 않았고 요구 여부도 미확인.
- **비압축의 여분 바이트 상한이 없다.** 선언보다 100배 큰 PixelData 도 여전히 성공이다.
  이것이 "패딩" 으로 볼 수 있는 범위인지 이 카드가 정하지 않았다.
- **커버리지는 측정하지 않았다** — x64 OpenCppCoverage 미설치 결정 유효.

---

## 8. 잔여 위험 (Residual-risk)

- **동작 변경이고, 이번에는 "성공하던 것" 을 거절한다.** 선언보다 큰 코드스트림을 정상으로
  취급하던 흐름이 현장에 있었다면 배포 후 드러난다. `!=` 는 `<` 보다 엄격하므로 표면이 넓다.
- **엄격해진 만큼 오탐 비용이 있다.** 실제 장비가 쓰는 인코더 중 치수를 올림 처리하는 것이
  있다면(예: 타일 경계 맞춤) 그 파일이 거절된다. **이 저장소의 픽스처는 모두 자체 writer
  산출물이라 그 가능성을 검증하지 못했다** — 실제 장비 파일 확보가 필요하다.
- **비압축과 압축의 답이 여전히 다르다.** 여분 바이트는 성공, 여분 치수는 실패 — §3 의
  근거로 의도한 차이이지만, 계약을 읽지 않은 사람에게는 비일관으로 보일 수 있다.
  헤더에 그 비대칭과 이유를 적어 두었다.
- 커밋은 push 전까지 미푸시 유일본이다.

---

## 부록 — 증거 파일

| 파일 | 내용 |
|---|---|
| `_env.bat` / `_verify.bat` | 환경 + 세 프리셋 재빌드·전체 ctest |
| `_b51.bat` | 관련 7건 빌드·실행 |
| `_observation.log` | **변경 전** — 큰 쪽 J2K `rc=0` / JPEG-LL `rc=-3` |
| `_red.log` | RED — BUILD=0, 큰 쪽 2건 실패 |
| `_green.log` | GREEN — BUILD=0, 7/7 |
| `_falsify.log` | `<` 로 약화 — BUILD=0, **큰 쪽 2건만** 재실패(**재현됨**) |
| `_verify.log` | 최종 465 / 209 / 173, 경고 0 |
