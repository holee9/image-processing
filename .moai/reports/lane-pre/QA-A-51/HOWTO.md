# 실프레임이 도착하면 — 실행법 (QA-A-51, #151)

이 문서만 읽고 돌릴 수 있게 썼다. 카드나 보고서를 먼저 읽을 필요 없다.

## 0. 준비 — 한 번만

```powershell
. ./tools/ci/Use-MsvcDevShell.ps1
cmake --build build/ci-preprocess --target xpe_real_frames xpe_struct_frames
```

바이너리: `build/ci-preprocess/bin/xpe_real_frames.exe`

## 1. 명령 한 줄

**DICOM 파일이면:**

```powershell
./build/ci-preprocess/bin/xpe_real_frames.exe --in frame01.dcm --in frame02.dcm --in frame03.dcm --inject
```

**원시(raw) 파일이면** — 치수와 비트 깊이를 반드시 같이 준다:

```powershell
./build/ci-preprocess/bin/xpe_real_frames.exe --raw 3072x3072:16 --in frame01.raw --in frame02.raw --inject
```

`--raw WxH:BITS` 에서 BITS 는 `8` / `16` / `32`(float32). 파일 크기가 치수와 안 맞으면
**건너뛰지 않고 이름과 함께 왜 안 되는지 찍는다.**

`--inject` 는 선택이다. 붙이면 실프레임 잡음 위에 **합성 결함을 얹어** 회수율을 같이 잰다.
그 수치는 출력에서 `INJECTED-ON-REAL` 로 표시되며 **실제 결함 검출 성능이 아니다.**

### DICOM 이 안 읽힌다고 나오면

`ci-preprocess` 프리셋은 `BUILD_DICOM=OFF` 라 `xpe_dicom.dll` 이 없다. 이 도구는 그 DLL 을
**링크가 아니라 실행 시점에** 찾는다. **스테이징 스크립트를 쓰면 된다** (QA-A-52 에서 실제로
DICOM 을 읽는 데 쓴 바로 그 경로다):

```powershell
powershell -NoProfile -File .moai/reports/lane-pre/QA-A-52/Stage-DicomRuntime.ps1
cd .moai/reports/lane-pre/QA-A-52/stage
./xpe_real_frames.exe --in <파일>.dcm --inject
```

스크립트가 harness 와 `xpe_dicom.dll`, 그리고 **DCMTK 전이 의존 DLL 11개**를 한 디렉터리에
모은다. 이걸 빼먹으면 `GetLastError()=126` 만 뜨고 **어느 DLL 이 없는지는 안 알려준다** —
그래서 없는 파일 목록을 스크립트가 대신 찍는다.

`--raw` 로 변환해 넘기는 길도 그대로 유효하다.

## 2. 출력 읽는 법

맨 위에 **사전 판정 규칙**이 먼저 찍힌다. 데이터를 보기 전에 고정한 숫자이고, 매 실행마다
같이 나온다. 실행 결과를 보고 이 숫자를 바꾸면 결론을 데이터에 맞추는 것이다.

프레임마다:

| 줄 | 뜻 |
|---|---|
| `local sigma : p01 .. max` | 이 프레임 자신의 국소 잡음 분포. 구조가 있으면 p90 이 크게 뜬다 |
| `global sigma: A / Bm / Bm4` | A=값 MAD(출고 이전), Bm=현행, Bm4=4방향 후보 |
| `Bm/medLocal`, `Bm4/medLocal` | 전역 추정이 프레임 자신의 잡음과 얼마나 맞는지 |
| `flagged`, `flaggedRate` | **결함 개수가 아니라 표시된 화소 수**다. 실프레임엔 정답이 없다 |
| `1% ok / OVER` | SPEC 의 1% 상한(`≤ W*H*0.01`). 이건 정답 없이도 판정된다 |
| `flagged by local-sigma decile` | d1=가장 조용한 10% … d10=가장 시끄러운 10%. **어디에 몰리는가** |
| `top-decile enrichment` | 1.0x=고르게 퍼짐, 10.0x=전부 가장 시끄러운 10%에 몰림 |

맨 아래 `VERDICT` 가 R1/R2/R3/R5 를 적용해 **REJECT / ACCEPT / INCONCLUSIVE** 중 하나를 찍는다.
`INCONCLUSIVE` 는 "기준을 낮춰라"가 아니라 **"프레임이 더 필요하다"** 는 뜻이다.

## 3. 잴 수 없는 것 — 먼저 알아둘 것

실프레임에는 **결함 위치 정답이 없다.** 그래서:

- **TPR 은 못 잰다.** 출력에 없다
- **FPR 도 문자 그대로는 못 잰다.** 표시된 화소가 진짜 결함일 수 있다. 그래서 이 도구는
  실프레임에 대해 `FP` 라는 말을 쓰지 않고 `flagged` 라고만 쓴다
- **잴 수 있는 것**: 표시 화소 수·비율, 그 **위치 분포**, 전역 σ 대 국소 σ, 설정 간 차이,
  그리고 SPEC 1% 상한

## 4. 하네스가 멀쩡한지 먼저 확인

실데이터를 믿기 전에 자체 검증을 한 번 돌린다.

```powershell
./build/ci-preprocess/bin/xpe_struct_frames.exe --dump .moai/reports/lane-pre/QA-A-51/frames
./build/ci-preprocess/bin/xpe_real_frames.exe --selftest .moai/reports/lane-pre/QA-A-51/frames
```

`SELF-TEST PASS` 가 떠야 한다. QA-A-47/A-49 모사 프레임을 이 도구 **자기 입력 경로**로 다시
읽어 QA-A-50 표와 대조한다. `FAIL` 이면 **하네스 결함이지 발견이 아니다** — 고치고 다시 돌린다.

## 5. 증거 남기기

```powershell
./build/ci-preprocess/bin/xpe_real_frames.exe --in <...> --inject 2>&1 |
    Tee-Object -FilePath .moai/reports/lane-pre/<카드>/real-frames.log
```

로그에 사전 판정 규칙이 함께 찍히므로, 그 파일 하나가 "무슨 기준으로 무엇을 쟀는가"의 증거가 된다.
