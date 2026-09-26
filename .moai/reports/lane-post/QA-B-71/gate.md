# QA-B-71 게이트 보고서 — 1단계에서 멈췄다

**카드**: QA-B-71 (#167) · **레인**: Lane B (`xpe-post`, `dev/postprocess`)
**커밋 1건**: `62d84c2` (측정 프로브만, 제품 코드 변경 없음) · **이슈 코멘트**: #167
**BUILD_EXIT**: `BUILD=0` · ctest **533 / 224 / 186**, 경고 0

---

## 1. 주장 (Claim)

| 질문 | 답 |
|---|---|
| 측정 파일이 어느 분기를 지나는가 | **전부 `:192`**(meta 있음 · TS 요소 없음). `:171`(meta NULL)에 도달한 파일 **0개** |
| `:192` 에서 감지 구문을 얻을 수 있는가 | **얻는다.** native 3종은 **정확**, 캡슐화 2종은 **틀린 값**(Explicit LE) |
| `:171` 에서 감지 구문을 얻을 수 있는가 | **미측정** — 도달하는 픽스처를 만들지 못했다 |
| 2단계(수정)를 했는가 | **하지 않았다** — 카드 §2 멈춤 조건 |

---

## 2. 증거 (Evidence) — `_step1.log`

```
Explicit VR LE (supported)      | branch=meta present, no TS element | written=…1.2.1     | detected=…1.2.1 | pixelData encapsulated=0 | match=1
Implicit VR LE (unsupported)    | branch=meta present, no TS element | written=…1.2       | detected=…1.2   | pixelData encapsulated=0 | match=1
Explicit VR BE (unsupported)    | branch=meta present, no TS element | written=…1.2.2     | detected=…1.2.2 | pixelData encapsulated=0 | match=1
.70 JPEG-LL (supported, encaps) | branch=meta present, no TS element | written=…1.2.4.70  | detected=…1.2.1 | pixelData encapsulated=1 | match=0
.57 JPEG-LL (supported, encaps) | branch=meta present, no TS element | written=…1.2.4.57  | detected=…1.2.1 | pixelData encapsulated=1 | match=0
detected syntax matched the written one in 3 of 5 measured files
```

### 2.1 방법

프로브는 `DicomReader::open()` 과 **같은 인자**(`EXS_Unknown`, `EGL_noChange`,
`DCM_MaxReadLength`)로 `loadFile` 한다. 리더 내부를 계측하지 않고 같은 호출을 재현했다 —
`open()` 은 로드 직후 `getMetaInfo()` 와 `findAndGetOFString(DCM_TransferSyntaxUID)` 로만
분기하므로, 같은 두 호출을 관찰하면 같은 분기가 결정된다.

### 2.2 대조군 — 첫 실행 뒤에 추가했다

첫 실행에서 JPEG 두 파일이 `detected=Explicit LE` 로 나왔다. 이것만으로는 **두 가지로 읽힌다**:

- DCMTK 가 캡슐화 구문을 감지하지 못한다
- 픽스처가 실제로는 비압축으로 쓰였고, Explicit LE 가 **맞는** 답이다

그래서 같은 로드에서 `PixelData` 가 캡슐화돼 있는지를 따로 확인하는 대조군을 붙였다
(`getEncapsulatedRepresentation` 또는 길이 필드 = undefined). **두 JPEG 파일 모두
`encapsulated=1`** — 픽스처는 캡슐화돼 있고, **감지값이 틀린 것**이다.

B-68·69 의 측정과도 맞는다: meta 없는 `.70`·`.57` 은 `read=-13`(native 로 캡슐화 데이터를
못 읽음), native 대비 화소 0.

---

## 3. 멈춘 이유 — 카드 조건과 대조

| 카드 조건 | 측정 | 해당 |
|---|---|---|
| "감지 구문을 얻을 수 **없는** 분기가 있으면 멈춤" | `:171` 은 **도달 못 함** — 얻을 수 있는지 모름 | ✔ |
| "노출한다면 그 값이 실제 구문과 **맞는지**" | `:192` 에서 캡슐화 파일은 **안 맞음** | ✔ |
| "추측으로 구현하지 마십시오" | — | 준수 |

**두 번째가 첫 번째보다 무겁다.** "얻을 수 없다" 는 구현하면 드러나지만, **틀린 값이
조용히 나온다** 는 구현해도 드러나지 않는다. 그리고 그 틀린 값(`1.2.840.10008.1.2.1`)은
**수용 목록에 있다.**

---

## 4. 측정에서 읽히는 것 (판정 재료 — 판정 아님)

카드가 "방식을 다시 정해야 한다" 고 했으므로, 재정의에 필요한 사실만 정리한다.

| 파일 종류 | 감지 | 감지값이 목록에 있나 | 대조 로직을 넣으면 |
|---|---|---|---|
| native 지원 (Explicit LE) | 정확 | 있음 | 수용 — 대조군 유지 |
| native 미지원 (Implicit LE, BE) | 정확 | 없음 | **거절** — B-69 누출 2건이 닫힘 |
| 캡슐화 (`.70`, `.57`) | **틀림 → Explicit LE** | 있음 | **수용** → native 읽기에서 `-13` — 지금과 동일 |

즉 감지값 대조를 넣으면 **측정된 누출 2건은 닫히지만**, 캡슐화 파일에 대해서는 **여전히
"우연히" 막힌다** — 감지가 Explicit LE 로 거짓말하고, 그 뒤 native 읽기가 구조 때문에
실패하는 것은 지금과 같다. 카드 §1 이 말한 "우연에 기댄 안전은 안전이 아니다" 가 이
부류에는 여전히 적용된다.

(이 표의 넷째 열은 **코드 판독에 의한 예상**이다. 구현하지 않았으므로 측정이 아니다.)

---

## 5. Baseline 귀속 (Baseline-attribution)

| 항목 | 값 | 근거 |
|---|---|---|
| 측정 파일 | 5 | `_step1.log` |
| `:192` 경유 | **5** | `_step1.log` |
| `:171` 경유 | **0** | `_step1.log` |
| 감지 일치 | **3 / 5** (native 전부) | `_step1.log` |
| 캡슐화 파일 감지값 | `1.2.840.10008.1.2.1` (2 / 2) | `_step1.log` |
| 캡슐화 대조군 | `encapsulated=1` (2 / 2) | `_step1.log` |
| 제품 코드 변경 | **0** | `git diff 788d8b5 -- modules/dicom/src` |
| 이전 ctest | 533 / 224 / 185 | QA-B-70 `_verify.log` |
| 현재 ctest | **533 / 224 / 186** (프로브 1건 추가) | `_verify.log` |
| 빌드 경고 | 0 | `_verify.log` |

---

## 6. 미검증 (Gaps)

- **`:171` 분기는 미측정.** preamble 없이 데이터셋만 저장한 파일로는 도달하지 못했다.
  어떤 입력이 `getMetaInfo() == NULL` 을 만드는지, 그런 입력이 존재하는지 확인하지 않았다.
  **도달 불가능한 분기일 가능성도 있지만 그것도 측정하지 않았다.**
- **DCMTK 의 다른 감지 수단은 조사하지 않았다.** `getOriginalXfer()` 하나만 봤다. 캡슐화를
  감지하는 다른 API(예: PixelData 구조를 보고 추정)가 있는지는 모른다 — 대조군에서 쓴
  `getEncapsulatedRepresentation` 은 **어느 구문인지 이미 알 때** 묻는 호출이다.
- **감지가 틀리는 이유는 조사하지 않았다.** DCMTK 가 meta 없는 스트림에서 무엇을 근거로
  구문을 정하는지(태그 VR 패턴 등) 소스를 보지 않았다. 캡슐화 여부가 그 판정에 들어가지
  않는 것으로 보이지만 **확인하지 않았다.**
- **Deflated** — 카드 범위 밖, 여전히 미측정.
- **합성 데이터다(#148).**

---

## 7. 잔여 위험 (Residual-risk)

- **B-69 가 측정한 누출 2건은 열린 그대로다.** 이 카드는 수정하지 않았다.
- **감지값을 믿는 구현은 조용히 틀린다.** 캡슐화 파일에서 `getOriginalXfer()` 가
  수용 목록에 있는 값을 돌려주므로, 대조 로직은 **그 파일을 통과시키고 아무 경고도 내지
  않는다.** 지금은 native 읽기 실패가 뒤에서 막지만, 그 막힘은 여전히 구조에 의존한다.
- **meta 는 있으나 TS 요소가 없는 파일이 이 경로의 실체다.** `#167` 본문과 여러 주석이
  "meta 없는 경로" 라고 부르지만, 측정된 파일은 전부 **meta 객체가 있는** 분기를 탔다.
  이름이 분기와 어긋나 있다.
- 커밋은 push 전까지 미푸시 유일본이다.

---

## 부록 — 증거 파일

| 파일 | 내용 |
|---|---|
| `_env.bat` / `_verify.bat` / `_verify.log` | 세 프리셋, 533 / 224 / 186, 경고 0 |
| `_b71.bat` / `_step1.log` | 프로브 표(대조군 포함), DISABLED 포함 실행 `EXIT=1`(B-69 의 의도된 실패) |
