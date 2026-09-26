# QA-B-69 게이트 보고서 — 우연히 막히는 것과 검사해서 막히는 것

**카드**: QA-B-69 (#167) · **레인**: Lane B (`xpe-post`, `dev/postprocess`)
**커밋 1건**: `bf141da` · **증거**: `.moai/reports/lane-post/QA-B-69/`
**BUILD_EXIT**: 기본 실행 `BUILD=0` / `EXIT=0` · DISABLED 포함 `BUILD=0` / `EXIT=1`(의도)

---

## 1. 주장 (Claim) — 답은 "화소가 나온다"

카드의 미지수는 하나였다: **캡슐화하지 않는 미지원 구문이 meta 없는 경로로 들어오면
화소가 나오는가.**

**나온다.** 지시대로 거기서 멈추고 고치지 않았다.

---

## 2. 증거 (Evidence)

### 2.1 측정 (`_b69.log`)

```
control: meta-less Explicit VR LE (SUPPORTED) open=0 read=0 pixels=1 256x256
Implicit VR Little Endian (1.2.840.10008.1.2):  with-meta open=-7 | meta-less open=0 read=0 pixels=1 256x256
Explicit VR Big Endian  (1.2.840.10008.1.2.2):  with-meta open=-7 | meta-less open=0 read=0 pixels=1 256x256
Deflated Explicit VR LE (1.2.840.10008.1.2.1.99): DCMTK cannot write this syntax here -- not measured
native unsupported syntaxes yielding pixels without a meta-header: 2 (#167)
```

| 구문 | 수용 목록 | meta 있음 | meta 없음 |
|---|---|---|---|
| Implicit VR Little Endian | ✘ | **-7** (거절) | **`open=0` `read=0` 화소 256×256** |
| Explicit VR Big Endian (폐기) | ✘ | **-7** (거절) | **`open=0` `read=0` 화소 256×256** |
| Deflated Explicit VR LE | ✘ | — | DCMTK 가 이 빌드에서 쓰지 못함 — **미측정** |
| Explicit VR Little Endian (대조군) | ✔ | — | `open=0` `read=0` 화소 256×256 |

**라벨이 붙으면 거절하는 파일을, 라벨이 없으면 읽는다.**

### 2.2 대조군 둘 다 살아 있다

카드가 요구한 두 대조를 같은 실행에 붙였고, 둘 다 **단언**으로 걸었다:

| 대조 | 역할 | 결과 |
|---|---|---|
| meta **있는** 같은 파일 → `-7` | 수용 목록 검사가 **동작 중**임을 보임. 없으면 위 결과가 "이 구문은 그냥 관대하게 허용된다" 로 읽힌다 | ✔ 두 구문 모두 |
| meta **없는 지원** 구문 → 화소 | 픽스처 작성기가 멀쩡함을 보임. 없으면 모든 "화소 없음" 이 픽스처 이야기가 된다 | ✔ |

지원 구문 대조는 **가장 먼저** 배치해 `ASSERT` 로 걸었다 — 작성기가 깨졌으면 그 아래는
아무것도 측정하지 않는다고 말한다.

### 2.3 후보는 목록이 아니라 DCMTK 에서 유도했다

카드 지시대로 "native(비캡슐화)이고 수용 목록에 없는 것" 이라는 **성질**로 뽑았다.
그리고 표의 "수용 목록에 있음" 표시를 **런타임에 `kSupportedTransferSyntaxes` 와 대조**해
어긋나면 먼저 멈춘다 — 표를 믿지 않는다("이름으로 센 census 는 census 가 아니다").

---

## 3. 반증 — B-68 의 추론이 측정이 됐다

카드가 지시한 반증은 "화소가 안 나온다는 결론이 나오면" 이었고, 실제로는 나왔다. 그래도
**막던 것이 무엇인지**는 별개 질문이라 그대로 수행했다 — 같은 meta 없는 경로에 캡슐화된
것과 native 인 것을 짝지어 넣었다:

```
meta-less ENCAPSULATED (.70):                 open=0 read=-13 pixels=0
meta-less NATIVE (Implicit LE, unsupported):  open=0 read=0   pixels=1
```

**막던 것은 구문 검사가 아니라 캡슐화 구조다.** B-68 이 코드를 읽고 추론으로 적은 문장이
실행으로 확인됐다. 두 파일은 **같은 원본 데이터셋**에서 나왔고 경로도 같으며, 다른 것은
픽셀 데이터의 캡슐화 여부뿐이다.

---

## 4. 두 극성으로 남겼다 — B-66 구조

| 케이스 | 역할 | 오늘 |
|---|---|---|
| `DISABLED_MetaLessNativeUnsupportedSyntaxesProduceNoPixels` | **되어야 할 것**을 단언 | **실패**(의도). 구멍이 막히는 날 초록 |
| `KnownDivergence_MetaLessPathLeaksNativeUnsupportedSyntaxes` | 같은 스윕을 **기본 실행에서** 기록 | 통과, `leaked=2` 를 로그에 남김 |

둘로 나눈 이유: `DISABLED_` 는 기본 실행에서 돌지 않으므로, 그 안에만 두면 **발견이 꺼진
테스트 안에 갇힌다.** 기록용 짝이 매 실행 로그에 수치를 남긴다.

`DISABLED_` 를 고른 이유는 미해결이 아니라 **막힘**이다 — 무엇을 어떻게 막을지가 판정이고,
거기에 "meta 없는 파일을 애초에 받을 것인가" 가 얽혀 있다. 거절할지, 감지된 구문을 검사할지,
현재 관용을 유지하고 문서화할지가 **서로 다른 제품**이다.

---

## 5. Baseline 귀속 (Baseline-attribution)

| 항목 | 값 | 근거 |
|---|---|---|
| 화소를 낸 미지원 native 구문 | **2** (Implicit LE, Big Endian Explicit) | `_b69.log` |
| 측정 못 한 후보 | **1** (Deflated — DCMTK 가 못 씀) | `_b69.log` |
| meta 있는 같은 파일 | **-7** (두 구문 모두) | `_b69.log` |
| 지원 구문 meta 없음(대조) | `open=0 read=0` 화소 256×256 | `_b69.log` |
| 반증: 캡슐화 vs native | 화소 **0 vs 1** | `_b69.log` |
| **DISABLED_ 총수** | **3** (display VOI, preprocess 성능, 이번 것) | `grep` |
| 이전 ctest | 533 / 224 / 183 | QA-B-68 `_verify.log` |
| 현재 ctest (기본) | **533 / 224 / 185** | `_verify.log` |
| 빌드 경고 | 0 | `_verify.log` |

---

## 6. 미검증 (Gaps)

- **오늘의 사실이지 불변식이 아니다.** 이것은 **오늘의 DCMTK 판**과 **오늘의 수용 목록**에
  대한 측정이다. 구문이 하나 추가되거나 DCMTK 가 Deflated 를 쓸 수 있게 되면 다시 참이
  아닐 수 있다 — 카드가 0건이었을 때 적으라 한 문장이지만, 2건인 지금도 같은 이유로 적는다.
- **Deflated Explicit VR LE 는 재지 못했다.** DCMTK 가 이 빌드에서 `canWriteXfer` 를
  거절한다. 이 구문은 압축 스트림이지만 캡슐화는 아니므로 **native 경로로 들어갈 수
  있는지 모른다** — 후보 중 유일하게 열려 있다.
- **화소가 "틀렸는지" 는 재지 않았다.** 나온 프레임이 원본과 일치하는지 비교하지 않았다.
  DCMTK 가 감지된 구문으로 올바르게 파싱했을 가능성이 높지만 **확인하지 않았고**,
  이 카드의 질문은 "나오는가" 였다.
- **meta 없는 파일이 실제로 유통되는지 모른다.** Part-10 preamble 없는 파일이 임상 경로에
  들어올 현실적 경로(네트워크 수신, 레거시 아카이브 등)는 조사하지 않았다.
- **#168(라벨 불일치)은 손대지 않았다** — 카드가 범위 밖으로 명시했다.
- **합성 데이터다(#148).** 전부 DCMTK 가 `s_validDcm` 에서 쓴 파일이고, 실장비가 만든
  meta 없는 파일이 같은 모양인지는 모른다.

---

## 7. 잔여 위험 (Residual-risk)

- **검사를 우회하는 입구가 남아 있다.** 이 카드는 측정만 했다. (정정 QA-B-70: 아래
  `:157` 은 당시 줄 번호이고, 측정한 파일이 meta NULL 분기(현재 `:171`)와 TS 요소 없음
  분기(현재 `:192`) 중 어느 쪽을 지났는지는 재지 않았다.) `DicomReader.cpp:157` 은
  여전히 meta 가 없으면 **Explicit VR Little Endian 이라고 기록한다** — 추측이고, 그
  추측이 틀린 파일이 화소를 낸다.
- **`.57` 지원(B-68)은 이 구멍을 넓히지도 좁히지도 않았다.** meta 없는 경로는 수용 목록을
  읽지 않으므로 목록이 길어져도 그 경로의 동작은 그대로다 — 다만 **목록이 안전의 근거로
  인용될 때마다 그 인용이 이 경로에서는 틀린다.**
- **`DISABLED_` 가 3개가 됐다.** 아직 신호지만, 늘어나면 숫자가 무뎌진다는 리더의 지적이
  그대로 적용된다.
- **폐기된 구문(Big Endian Explicit)이 읽힌다는 것이 특히 나쁘다.** DICOM 이 폐기한 구문을
  라벨 없이 받아 읽으면, 그 파일이 어디서 왔는지에 대한 정보가 전혀 없는 채로 화소가
  파이프라인에 들어간다.
- 커밋은 push 전까지 미푸시 유일본이다.

---

## 부록 — 증거 파일

| 파일 | 내용 |
|---|---|
| `_env.bat` / `_verify.bat` / `_verify.log` | 세 프리셋, 533 / 224 / 185, 경고 0 |
| `_b69.bat` / `_b69.log` | `BUILD=0`, DISABLED 포함 `EXIT=1`(의도), 위 측정값 전부 |
