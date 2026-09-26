# GUI-C-09 — 클라이언트가 `XpeImageBuffer.dataSize` 를 채우는지 확인

- 카드: GUI-C-09 · Refs #123
- 워크트리: `D:/workspace-github/xpe-gui` / 브랜치 `dev/gui` / HEAD `a3ae133` (main 병합 완료)
- **결론: 채울 것이 없다. 8개 생성 지점 전부 이미 정확한 값을 넣고 있다. 코드 무변경**
- 카드 배경 1건 정정 필요 — §1

---

> **[2026-09-10 정정 — leader 판정 후]** 아래 §1 의 관측(병합된 main 에 계약 절이 없다)은
> 그대로 유효하나, **원인은 "절이 존재하지 않는다" 가 아니라 "아직 push 되지 않았다" 였다.**
> leader 확인: 계약 절은 커밋 `379432f`(로컬 `main` `9457de9`)에 있고 `origin/main` 에는
> 미반영이다. 이 레인이 볼 수 있는 범위에서는 부재가 맞지만, **"없다" 가 아니라
> "origin/main 에 아직 없다" 가 정확한 진술**이다. 아래 본문은 관측 기록으로 보존한다.
>
> 부족 `dataSize` 케이스는 네이티브 가드(QA-B-20/A-17) 통합 뒤 GUI-C-10 으로 발행 예정.

## 1. 먼저 — 카드 배경의 전제가 확인되지 않는다 (관측 시점: `a3ae133`)

카드는 "`api-spec` 에 `dataSize` 입력 계약이 생겼다(`docs/project/api-spec.md`, 2026-09-10)"
라고 적었다. 병합된 main(`a3ae133`)에서 실측하면 **그런 절이 없다.**

```
grep -n "dataSize" docs/project/api-spec.md
  58:  size_t dataSize;  /* Byte size of data buffer; max 64 MB (4096x4096x4) */
  246: ... zeroes buf->data and buf->dataSize. Passing a zero-initialised buffer is a no-op.
```

두 줄 다 이전부터 있던 것이고, "0 은 허용하되 미초기화는 UB", "`0 < dataSize < w*h*bpp` 거절"
같은 문장은 **어느 쪽 api-spec 사본에도 없다**(`docs/project/`, `.moai/project/` 양쪽 확인).
최근 api-spec 커밋 3건은 오류코드 우선순위(`666e6dc`, `11b12c5`)와 핸들 기반 모듈
(`cbd5fcf`) 건이다.

**다만 조사 작업 자체는 그 문서와 무관하게 성립한다.** `dataSize` 의 의미
("width × height × bytes_per_pixel")는 `docs/common/xpe-common-prd.md:197,614` 에 이미 있고,
그 기준으로 전수 조사했다. 계약 절이 실제로 필요하다면 별도 카드 사안이다.

## 2. 전수 조사 — `XpeImageBuffer` 대응 구조체를 만드는 곳

C# 쪽 선언은 3개다: `PInvokeWrapper.XpeImageBuffer`(nuint DataSize),
`XpeCommonNative.XpeImageBuffer`(nuint), `XpeDisplayInterop.XpeImageBufferNative`(UIntPtr).

| # | 지점 | `dataSize` 값 | 판정 |
|---|---|---|---|
| 1 | `Diagnostics/XpeEnhanceBasicReadinessProbe.cs:231` | `pixelCount * sizeof(float)`, 호출부 32×32 | **정확** |
| 2 | `Diagnostics/XpePreprocessSyntheticOracle.cs:249` | 인자 `dataSize`, 호출부 6곳 모두 `배열.Length × 원소크기` | **정확** |
| 3 | `Services/NativeEnhanceBasicPreviewService.cs:348` | `output.Length * sizeof(float)` | **정확** (§3) |
| 4 | `Services/NativePreprocessPreviewService.cs:987` | 인자 `dataSize`, 호출부 6곳 모두 `배열.Length × 원소크기` | **정확** |
| 5 | `IntegrationTests/Fixtures/ImageBufferFactory.cs:37` | `count * sizeof(ushort)`, 16×16 | **정확** |
| 6 | `IntegrationTests/P1AReady/PreprocessCorrectionChainSmokeTests.cs:210` | 인자 `dataSize`, 호출부 6곳 모두 `배열.Length × 원소크기`, 배열은 `width*height` | **정확** |
| 7 | `Services/NativePresentationExportService.cs:110` | `default(...)` → 즉시 `xpe_alloc_image(out buffer)` | **네이티브가 채움** |
| 8 | `gui/Services/RealXpeBackend.cs:109` | `default(...)` → 즉시 `xpe_alloc_image(out image)` | **네이티브가 채움** |

부가 항목 2개(네이티브 호출 없음):
- `IntegrationTests/Functional/StructLayoutParityTests.cs:26` — `DataSize = 614400` 고정.
  마샬링 왕복 검사용이며 네이티브에 넘기지 않는다
- `Fixtures/ImageBufferFactory.CreateEmpty()` — `default` 반환이라 `DataSize = 0`.
  **호출부 0건인 죽은 API** (§5)

**미설정으로 남는 지점은 없다.** `default(...)` 2곳은 `out` 인자로 곧바로 네이티브 할당에
넘어가므로 클라이언트가 값을 만들 여지 자체가 없다.

## 3. 정확성 근거 — "채웠다" 와 "맞다" 는 다르다

`DataSize` 를 넣는 코드가 있다는 것만으로 정확하다고 하지 않았다. 값이 `width×height×bpp` 와
일치하는지 호출 사슬을 끝까지 따라갔다. 가장 긴 것이 3번이다.

```
CreateFloatBuffer(preview.PreviewWidth, preview.PreviewHeight, output.Length)
  output.Length == preview.SampledPixels.Length            (NativeEnhanceBasicPreviewService.cs:75 가드)
  SampledPixels = new ushort[previewWidth * previewHeight] (RawPreviewService.cs:204)
  → DataSize = previewWidth*previewHeight*4, Format=Float32/BitsAllocated=32 와 정합
```

1·5번은 리터럴 크기(32×32, 16×16)라 직접 확인된다. 2·4·6번은 호출부 18곳 전부
`배열.Length × 원소크기` 형태이고 배열은 모두 `width*height` 로 할당된다.

1·3번은 생성 시점에 `Data` 가 `IntPtr.Zero` 로 남는데, 두 호출부 모두 바로 다음 줄에서
`buffer.Data = handle.AddrOfPinnedObject()` 를 대입한다(`:135`, `:154`). 즉 `dataSize > 0` 인데
`data` 가 널인 채로 네이티브에 넘어가는 경로는 없다.

## 4. 재실측

```
dotnet test clients/ImageProcTest.IntegrationTests/ImageProcTest.IntegrationTests.csproj -c Debug
실패! - 실패: 6, 통과: 84, 건너뜀: 0, 전체: 90
```

`build/ci-common/bin/` 에 CI 와 동일한 8개 DLL 스테이징 상태. **GUI-C-08 직후 수치와 동일**
하며, main 병합(`a3ae133`)으로 인한 회귀는 없다. 실패 6건은 GUI-C-08 §5 의 그 6건으로
동작 불일치이며 이 카드와 무관하다. 로그: `step4-tests.log`

## 5. 미검증 (Gaps)

- **네이티브가 실제로 `dataSize` 를 어떻게 쓰는지 확인하지 않았다.** 카드가 말한
  "`0 < dataSize < w*h*bpp` 거절" 가드가 현재 모듈에 있는지 여부는 `modules/**` 소유
  경계 밖이라 조사하지 않았다. 따라서 "지금 값이면 향후 가드를 통과한다"는 주장은 하지 않는다
- 3단계 항목(부족 `dataSize` → `INVALID_INPUT` 테스트 추가)은 카드 지시대로 **하지 않았다**
  (QA-B-20/A-17 통합 뒤로 유예)
- `gui/ImageProcTest.SelfCheck`, `gui/ImageProcTest.E2E` 는 `XpeImageBuffer` 를 참조하지 않아
  조사 대상에서 제외했다 — grep 결과 0건이 근거이지, 두 프로젝트를 실행해 확인한 것은 아니다
- Release 구성 미빌드, WPF 앱 미실행
- 6건 실패는 2026-08-18 로컬 산출물 기준이다(GUI-C-08 과 동일한 한계)

## 6. 잔여 위험 / 관찰

- `ImageBufferFactory.CreateEmpty()` 는 호출부가 0건이다. `SkipHelper.ShouldSkip` 과 같은
  형태의 죽은 API 이며, "빈 버퍼 = `dataSize` 0" 을 검증하려던 의도가 배선되지 않은 채 남아
  있다. 이 카드는 조사·채우기 범위라 손대지 않았다 — 별도 판단 사안
- 3개 구조체 선언이 병행 존재한다(`nuint` 2개, `UIntPtr` 1개). x64 에서 동일 크기라 현재는
  문제가 없고 `AbiLayoutTests` 가 offset 32 를 고정 검증하지만, 선언이 갈라져 있다는 사실
  자체는 향후 필드 추가 시 세 곳을 모두 고쳐야 함을 뜻한다

## 부록 — 사용한 명령

```bash
git merge origin/main                     # → a3ae133 (fast-forward, .claude 변경 0건)
grep -rn "XpeImageBuffer\|DataSize" --include=*.cs --exclude-dir=obj --exclude-dir=bin clients gui
grep -n "dataSize" docs/project/api-spec.md .moai/project/api-spec.md
git log --oneline -5 -- docs/project/api-spec.md
export PATH="/c/Program Files/dotnet:$PATH"
dotnet test clients/ImageProcTest.IntegrationTests/ImageProcTest.IntegrationTests.csproj -c Debug
```
