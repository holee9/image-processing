# GUI-C-196 M2 — enhance_basic 단계 (네이티브)

범위: 기준선 체인의 두 번째 단계. EI-0(보정 안 된 EI 측정)은 M2 에 넣지 않았다 — 아래 "남긴 것" 참조.

## 만든 것

| 파일 | 역할 |
|---|---|
| `gui/ImageProcTest/Services/EnhanceBasicStage.cs` | 단계 논리. 네이티브 없음. 작업 영상 인터페이스(`IEnhanceImage`)로 호출을 숨겨 가짜로 시험한다 |
| `gui/ImageProcTest/Services/Native/XpeEnhanceBasicInterop.cs` | `xpe_log_transform` / `xpe_noise_reduce` / `xpe_contrast_enhance` / `xpe_edge_enhance` 선언과 구조체 3종 |
| `gui/ImageProcTest/Services/Native/GuiEnhanceNative.cs` | float32 버퍼 1개를 `xpe_alloc_image` 로 잡고 제자리 호출, 끝에 한 번 읽음 |
| `RealXpeBackend.cs` | `StageIds.EnhanceBasic` 분기 + `InvokeNative`(알림 배출 경로 동일) |
| `MockXpeBackend.cs` | 명시적 거절 문구 |
| `GuiNativeLibraryResolver.cs` | `xpe_enhance_basic.dll` 후보 경로 추가 |

## 단계의 규칙

- 한 장의 float32 영상에서 log → 노이즈(양방향) → 대비(CLAHE) → 모서리(USM) 순서로 실행하고, 16비트로는 **맨 끝에 한 번만** 바꾼다.
- 어느 한 단계라도 모듈이 거절하거나 NaN/Inf 를 남기면 단계 전체가 거절이고 픽셀은 돌려주지 않는다(부분 결과 없음).
- 거절 문구에 어느 단계에서, 어떤 반환 코드로 거절됐는지와 그 앞 단계들의 소요 시간이 들어간다.

## 반올림 규칙 표 (기준선 체인에서 어디에 무엇을 쓰는가)

규칙은 통일하지 않았다(리더 결정). 어디에 어떤 규칙이 있는지만 적는다.

| 변환 지점 | 규칙 | 출처 |
|---|---|---|
| 전처리 단계 끝: float → uint16 | 프레임 최댓값으로 정규화(`65535/max`) 후 **버림(truncate)** | `GuiPreprocessRunner.ReadFloatsAsUInt16` (읽어서 확인) |
| enhance_basic 단계 끝: float → uint16 | **반올림(half-to-even)**, 0..65535 로 자르고 자른 개수를 요약에 기록 | `EnhanceBasicStage.Run`, `MathF.Round(..., ToEven)` |
| 표시 파이프라인의 프리젠테이션 LUT 인덱스 | **반올림(half-away-from-zero)**, `roundf` | `modules/display/.../display_internal.h:123` (읽어서 확인) |

참고: 두 번째 줄과 세 번째 줄은 모양이 닮았지만 소비하는 쪽이 다르다. 전자는 저장되는 16비트 값, 후자는 1024 칸 LUT 의 색인이다.

## 측정 (판정 아님)

로컬 DLL(`build/ci-common/bin`, 2026-09-26 빌드본)로 3072×3072, 합성 영상:

```
MEASURED 3072x3072 enhance_basic: run 1 309 ms, run 2 290 ms
steps: log 27 ms; noise(bilateral 3/50) 91 ms; contrast(clahe 3 8x8) 75 ms; edge(usm 0.5/2/10) 39 ms
```

- 이 수는 **내 개발 기계**의 것이고 CI 러너의 값이 아니다. 3000 ms 예산 판정에 쓰지 않는다. 예산은 전체 기준선(전처리 + 이 단계 + 쓰기)에 걸린 것이며, M4 에서 전체 경로로 잰다.
- 합성 영상(경사 + 원판 + 시드 잡음)이다. 실제 DICOM 에서의 시간은 미측정.

## 검증

- 로컬: 신규 18건 통과 (가짜 모듈 12건 + 실제 모듈 3건 + 문서·배선 대조 2건 + 측정 1건). 실제 모듈 시험은 `build/ci-common/bin` 의 DLL 로 돌았다 — **스킵이 아니다**(출력에 `xpe_enhance_basic version: 1.0.0`).
- 전체 Functional 스위트: 448 통과, 0 실패, 1 건너뜀 (M1 때 430 + 신규 18). **중간에 34건이 빨강이었다**: 처음 버전은 시험 어셈블리에 DllImport 해석기를 설치했는데, 링크한 interop 소스가 그 어셈블리로 컴파일되므로 다른 네이티브 시험의 기본 탐색을 대체해 버렸다. 해석기를 없애고 두 DLL 을 전체 경로로 선로드하는 방식으로 바꿨다. 반증 1–11·8b 는 이 수정 **전** 버전으로 돌렸고 수정 후 재실행하지 않았다(바뀐 것은 시험의 DLL 적재 방식뿐이며, 수정 후에도 네이티브 시험 4건이 `native directory:` 를 찍고 통과하는 것은 확인했다).
- 반증 12건(번호 1–11 + 8b), 전부 빨강 후 바이트 동일 복원, 복원 후 18/18 — `m2_falsification_arms.txt`.
  - 8번은 첫 시도가 **빌드 실패**라 반증이 아니었다(`short` 로 바꾸면 컴파일 오류). 컴파일되는 8b(필드 순서 교체)로 다시 해서 실제 모듈 시험 3건이 빨강이 되는 것을 봤다.
  - 반증 5(해제 누락)가 빨강을 내는 이유: 가짜 모듈의 `Disposes` 계수 단언 때문이다. 네이티브 버퍼 누수 자체를 본 것은 아니다.

## 미검증·한계

1. **CI 에서 이 네이티브 시험이 도는가**: C# 통합 테스트 잡은 `xpe-ci-post-binaries` 를 받아 `build/ci-post/bin` 에 두는 것으로 읽었으나(`xpe_enhance_basic.dll` 확인 항목 있음), 시험의 DLL 탐색은 `XPE_NATIVE_DIR` → `build/ci-common/bin` → `build/ci-post/bin` 순이다. 그 잡 로그에서 `native directory:` 출력과 비-스킵 결과를 **푸시 뒤에 읽어야** 안다. 스킵으로 끝나면 시험이 초록이어도 네이티브 검증은 없던 것이다.
2. `xpe_common.dll` 의 로컬 빌드(9/26)와 `xpe_enhance_basic.dll` 빌드(9/26)가 서로 같은 커밋인지는 확인하지 않았다. 이 로컬 통과는 "그 두 파일 조합에서"만 말한다.
3. `Native E2E`(`gui-e2e-native`)는 이 단계를 아직 부르지 않는다(M4 이전에 명령이 없다). 그 잡의 staging 은 common/preprocess/post 를 한 폴더에 모으므로 DLL 은 들어간다고 읽었으나 **실행으로 확인한 것은 아니다**.
4. 앱의 `GuiNativeLibraryResolver` 가 실제로 이 DLL 을 찾는 경로는 시험 어셈블리에서 돌지 못한다(어셈블리당 resolver 1개). 배선은 소스 문자열 대조로만 확인했다 — M4 E2E 가 실행으로 확인할 몫.
5. NLM 모드 구조체 필드(`SearchWindow`, `PatchSize`, `HParam`)에는 모듈 기본값을 넣었으나 양방향 모드에서는 쓰이지 않는다고 헤더로 읽었다. 값이 틀려도 이 단계에서는 영향이 없다는 것은 **읽어서** 안 것이고 시험하지 않았다.

## 남긴 것 (M2 밖)

- **EI-0 (보정 안 된 EI 측정)**: 결정 D1 은 EI 를 전처리 단계 안에서 float 으로 재라고 했다. 전처리 러너의 float 출력 지점에 닿아야 하는데 `RunStages` 가 float 을 uint16 으로 바꾼 뒤에만 돌려준다. 이 변경은 평소 Apply 의 경로에 손을 대므로 M2(enhance 단계)와 분리해 **M4 에서 기준선 모드 전용 인자로** 넣는 것이 안전하다고 판단했다. 리더가 M2 에서 원했다면 알려 주시라.
- DICOM 쓰기·재읽기: M3.
