# QA-B-161 후속 — `ai-onnx` **CI 관측** (그리고 제 앞선 측정의 정정)

## 1. 주장

1. **관측됐습니다.** `ai-onnx` 잡이 main `a2d6407` 에서 `success` 로 끝났고,
   **248/248**, 제 반증 시험 두 건이 **CI 로그에 실행 기록으로** 있습니다. §2
2. **가드도 CI 에서 실제로 돌았습니다** — `XPE_AI_EXPECT_ONNX=1` 아래
   `AFullBuildIsProvableWhenTheCallerSaysItExpectedOne` 이 `Passed` 입니다.
   즉 이 잡이 **풀 빌드임이 잡 자신에 의해 증명**됩니다. §2
3. **제 `QA-B-161` 보고의 근거 하나가 무효였습니다.** *"측정된 부재"* 라고 적은 두
   검사 중 `git show` 쪽은 **지금도 0 을 냅니다** — 대상이 실재하는데도. §3
4. `text-lint` 가 같은 런에서 빨강인데 **제 파일이 아닙니다**(lane-gui). §4

---

## 2. §4 관측 — 추론이 아니라 로그

런 `36553270336`(main `a2d6407`), 잡 `ai-onnx` → **`completed/success`**.
로그는 작업 API 로 직접 받았습니다(`gh api .../actions/jobs/109356610423/logs`,
133,669 B) — 런이 아직 진행 중이라 `gh run view --log` 는 0줄을 돌려줬습니다.

### 런타임 획득

```
onnxruntime-win-x64-1.30.0
ONNX Runtime: using pre-built from D:\a\image-processing\image-processing\_deps\onnxruntime-win-x64-1.30.0
```

`QA-B-160` 이 보고한 조달 경로(사전 빌드 · `ONNXRUNTIME_ROOT`)가 **러너에서
그대로 동작했습니다.** 다운로드 URL·압축 해제 경로·변수 전달 — 제가 §5 에서
*"깨질 수 있다"* 고 적은 세 자리 모두 문제없었습니다.

### 시험 — 실행 기록

| 시험 | CI 결과 |
|---|---|
| **`OnnxSessionRun.RunningADifferentModelChangesTheOutput`** | **Passed 0.05 s** |
| **`BoneSuppressAbi.ADifferentModelDirectoryChangesTheOutput`** | **Passed 0.05 s** |
| `OnnxSessionBuildMode.AFullBuildIsProvableWhenTheCallerSaysItExpectedOne` | **Passed 0.01 s** |
| `BoneSuppressAbi.MissingModelAndBrokenModelGiveDifferentCodes` | Passed 0.05 s |
| `BoneSuppressAbi.ReInitWithADifferentDirectorySwitchesTheModel` | Passed 0.05 s |
| 총계 | **100% tests passed out of 248**, 5.26 s |

**건너뛴 것은 2건**이고 **둘 다 스텁 계약 시험**입니다 —
`OnnxSessionRun.StubBuildRefusesToRunAndDoesNotEcho`,
`BoneSuppressAbi.StubBuildFailsAndLeavesTheOutputAlone`. 풀 빌드에서 건너뛰는
것이 정확합니다.

> **이것이 `QA-B-160` 이 미검증으로 남긴 가장 큰 항목의 종결입니다.**
> 그때 저는 *"신규 시험이 CI 에서 건너뛴다"* 를 **프리셋과 로컬 실행에서 추론**했고
> 관측이 아니라고 적었습니다. 지금은 반대가 관측됐습니다 — **건너뛰는 것은 스텁
> 계약 2건뿐이고, 추론 경로를 단언하는 시험은 실제로 실행됩니다.**

### 가드가 왜 결정적인가

`ctest` 는 건너뜀을 통과로 셉니다. 만약 이 잡이 어떤 이유로든 스텁을 지었다면
248건이 모두 초록인데 **핵심 단언은 하나도 안 돈 상태**가 됩니다 — `#205` 의
모양입니다. `XPE_AI_EXPECT_ONNX=1` 아래 가드가 `Passed` 라는 것은 **그 잡이 스텁이
아님을 잡 스스로 증명**한다는 뜻입니다. 로그 한 줄이 그 구별을 대신합니다.

---

## 3. 제 앞선 측정의 정정 — **근거 하나가 무효였습니다**

`QA-B-161` 보고서 §6 에 *"추론이 아니라 측정된 부재"* 라고 적고 두 가지를 인용했습니다.
리더가 자기 쪽 같은 검사가 깨졌다고 알려 와 **제 것도 다시 쟀습니다**:

| 검사 | 그때 | 지금(대상 실재) | 판정 |
|---|---|---|---|
| **A** `MSYS_NO_PATHCONV=1 git show origin/main:... \| grep -c` | — | **1** | 정상 |
| **B** `git show origin/main:... \| grep -c` (제가 쓴 것) | 0 | **0** | **깨져 있음** |
| **C** `git branch -r --contains 5215e0f \| wc -l` | 0 | **2** | 정상 |

**B 는 대상이 실재하는 지금도 0 을 냅니다.** Git Bash 가 `origin/main:.github/...`
의 콜론·슬래시를 Windows 경로로 바꿔 `git show` 가 실패하고, 제가 붙인
`2>/dev/null` 이 그 실패를 가렸습니다. **부재를 측정한 것이 아니라 도구가 실패한
것을 0 으로 읽었습니다.**

**결론 자체는 살아 있습니다** — C 가 경로와 무관하고 그때 0(=어느 원격 브랜치에도
없음)을 냈으며, 지금 2 를 냅니다. 그러나 **제가 인용한 두 근거 중 하나는 아무것도
증명하지 않았고, 저는 그것을 "측정" 이라고 적었습니다.**

> 이것은 `QA-B-153`(파이프 뒤 `%errorlevel%`)·`QA-B-154`(`BUILD_TESTS` 오타에
> `No tests were found` + 종료코드 0)와 **같은 형태의 세 번째**입니다. 공통점은
> **도구 실패와 대상 부재가 같은 값으로 보인다**는 것이고, `2>/dev/null` 이 그
> 구별을 지웠습니다. **부재 주장에는 대조군이 필요하다**를 이번엔 *"같은 명령이
> 실재하는 것을 찾아내는가"* 로 적용했어야 했습니다.
>
> 오늘 같은 자리가 한 번 더 있었습니다: 로컬 `text-lint` 재현에서 `pwsh` 가 없어
> `9009` 가 났는데, **그것을 린트 실패로 읽지 않고** 도구 부재로 갈라
> `powershell.exe` 로 다시 쟀습니다(§4).

---

## 4. 같은 런의 `text-lint` 빨강 — **제 것이 아닙니다**

```
ERROR: .moai/reports/lane-gui/GUI-C-152/measured.txt:32 contains trailing whitespace.
  ... :42 :51 :85 :102
Tracked text file validation failed with 5 error(s).
```

5건 전부 **`lane-gui`** 파일입니다. 대조로 제 이번 커밋 파일들을 같은 패턴으로
훑어 후행 공백 **0건**을 확인했습니다. 다른 레인 소유라 **고치지 않고 보고만**
합니다.

`text-lint` 의 나머지 두 검사(`check_header_docs.py`, `check_spec_test_refs.py`)는
로컬에서 **둘 다 0** 입니다 — 제가 이번에 헤더 주석과 시험을 많이 건드렸으므로
확인했습니다.

## 5. 검증

```
잡 상태:    ai-onnx  completed/success        (gh run view --json jobs)
로그:       133,669 B                          (gh api .../jobs/109356610423/logs)
총계:       100% tests passed out of 248, 5.26 sec
반증:       OnnxSessionRun.RunningADifferentModelChangesTheOutput      Passed 0.05 sec
            BoneSuppressAbi.ADifferentModelDirectoryChangesTheOutput   Passed 0.05 sec
가드:       OnnxSessionBuildMode.AFullBuild...ExpectedOne              Passed 0.01 sec
건너뜀:     2건, 둘 다 스텁 계약 시험
런타임:     onnxruntime-win-x64-1.30.0, ONNXRUNTIME_ROOT 경로 로그에 출력
```

## 6. 미검증 · 잔여 위험

- **캐시 적중은 관측하지 못했습니다.** 이번이 첫 실행이라 `cache-hit` 이 없습니다 —
  **두 번째 실행에서야** 키가 작동하는지 알 수 있습니다
- **잡 소요 시간을 재지 않았습니다**(러너 점유 비용). 시험 실행은 5.26 s 이지만
  다운로드·빌드를 포함한 잡 전체 시간은 안 봤습니다
- **이 관측은 한 번의 런입니다.** 반복 실행의 안정성(특히 다운로드)은 미지입니다
- `text-lint` 빨강은 **다른 레인 소유**라 고치지 않았고, 그것이 main 을 빨갛게
  유지합니다
- `#226`(`/wd4150` 잔여)은 리더가 세웠고 **이 관측과 무관**합니다

---

Refs #130
