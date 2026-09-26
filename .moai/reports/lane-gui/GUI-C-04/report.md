# GUI-C-04 — `clients/ImageProcTest` 플랫폼 설정 x64 정렬

- 카드: GUI-C-04. **상태: 완료 (lead 지정 합격 조건 3건 전부 충족)**
- 워크트리: `D:/workspace-github/xpe-gui` / 브랜치 `dev/gui`
- Refs: #101 (솔루션 구성 정비의 잔여 항목)
- 선행: GUI-C-03 에서 Lane C 가 잔여로 남긴 항목
- 변경 파일 1건: `clients/ImageProcTest/ImageProcTest.csproj`

---

## 1. 변경 내용

```diff
   <PropertyGroup>
     <OutputType>WinExe</OutputType>
     <TargetFramework>net8.0-windows</TargetFramework>
+    <Platforms>x64</Platforms>
+    <PlatformTarget>x64</PlatformTarget>
     <Nullable>enable</Nullable>
     <UseWPF>true</UseWPF>
     <ImplicitUsings>enable</ImplicitUsings>
   </PropertyGroup>
```

정비 전에는 저장소 내 5개 csproj 중 이 프로젝트만 플랫폼 미지정(AnyCPU)이었다.

| 프로젝트 | 변경 전 | 변경 후 |
|----------|---------|---------|
| `clients/ImageProcTest` | (미지정 → AnyCPU) | **x64** |
| `clients/ImageProcTest.IntegrationTests` | x64 | x64 |
| `gui/ImageProcTest` | x64 | x64 (무수정) |
| `gui/ImageProcTest.E2E` | x64 | x64 (무수정) |
| `gui/ImageProcTest.SelfCheck` | x64 | x64 (무수정) |

### 지시 범위와의 차이 (고지)

지시서는 `<Platforms>x64</Platforms>` 를 명시했다. `<PlatformTarget>x64</PlatformTarget>` 도 함께 추가했다.
근거: 나머지 4개 프로젝트가 **두 속성을 모두** 보유하므로, 하나만 넣으면 "정렬"이 완결되지 않는다.
되돌리기는 한 줄 삭제로 끝난다. lead 가 원치 않으면 즉시 제거 가능.

`gui/` 4개 csproj 는 지시대로 손대지 않았다.

---

## 2. 검증 — lead 지정 합격 조건 3건

SDK: `dotnet 9.0.315`. 변경 전 `clients/ImageProcTest/{bin,obj}` 를 삭제하고 클린 리빌드했다.

### 2-1. 빌드 경고 0 유지 ✅

```
dotnet build clients/ImageProcTest.slnx -c Debug          → exit=0
    경고 0개
    오류 0개
```
```
ImageProcTest.IntegrationTests -> clients\ImageProcTest.IntegrationTests\bin\Debug\net8.0\ImageProcTest.IntegrationTests.dll
ImageProcTest                  -> clients\ImageProcTest\bin\Debug\net8.0-windows\ImageProcTest.dll
```
로그: `D:/workspace-github/xpe-gui/.moai/reports/lane-gui/GUI-C-04/build-clients.log`

### 2-2. 테스트 90/90 유지 ✅

```
dotnet test clients/ImageProcTest.slnx -c Debug --no-build → exit=0
통과!  - 실패: 0, 통과: 90, 건너뜀: 0, 전체: 90, 기간: 298 ms
```
로그: `D:/workspace-github/xpe-gui/.moai/reports/lane-gui/GUI-C-04/test-clients.log`

### 2-3. 출력 경로 불변 ✅ (지시 외 자체 추가 검증)

플랫폼 지정이 `bin/x64/Debug/` 형태로 경로를 바꾸면, GUI-C-02 에서 내가 목록화한 스크립트·문서 참조
(`tools/ci/Build-DicomModule.ps1:138`, `tests/test_data/calibration_cases/README.md:45,51`, `README.md:473-474`)가
전부 깨진다. 그래서 별도로 확인했다.

```
find clients/ImageProcTest/bin -maxdepth 2 -type d
  clients/ImageProcTest/bin
  clients/ImageProcTest/bin/Debug
  clients/ImageProcTest/bin/Debug/net8.0-windows      ← x64 하위 폴더 생성되지 않음
```

**경로 불변 확인.** SDK 스타일 프로젝트는 `Platform` 을 출력 경로에 붙이지 않는다 (기존 x64 프로젝트 4개도 모두 `bin/Debug/` 를 쓴다). 위 스크립트·문서는 수정 불필요하다.

---

## 3. 산출물 수준 효과는 입증하지 못했다 (중요)

변경이 실제로 산출물을 바꿨는지 PE 헤더로 확인하려 했다.

```
clients/ImageProcTest/bin/Debug/net8.0-windows/ImageProcTest.dll      PE machine = 8664 (x64)
clients/ImageProcTest.IntegrationTests/.../*.dll                       PE machine = 8664 (x64)
gui/ImageProcTest/.../ImageProcTest.dll                                PE machine = 8664 (x64)
```

변경 후 산출물이 x64 인 것은 확인했다. 그러나 **변경 전 값을 확보하지 못했다** — 검증 전에 `bin`/`obj` 를 삭제해 버려 이전 산출물이 남아 있지 않다.

사후 재현을 두 번 시도했고 둘 다 결론에 이르지 못했다:

1. `-p:PlatformTarget=AnyCPU -o <scratch>` — PE machine 이 여전히 `8664` 로 나왔다. 기존 `obj/` 를 재사용해 재컴파일이 일어나지 않았을 가능성이 크다. 대조군으로 쓸 수 없다.
2. `-p:BaseIntermediateOutputPath=<scratch>` 로 중간 산출물까지 분리 — WPF 의 `*_wpftmp.csproj` 임시 프로젝트 메커니즘과 충돌해 `CS0111` 164건으로 빌드 실패. 프로젝트 결함이 아니라 재현 방법의 한계다.

> **정직한 결론**: "이 변경으로 산출물이 AnyCPU→x64 로 바뀌었다"고 **주장하지 않는다.** 관측된 것은 "변경 후 x64" 뿐이다.
> 실제로 .NET 8 AnyCPU 앱은 x64 Windows 에서 이미 64비트로 기동하므로, 이 변경의 실질 가치는 **런타임 동작 개선이 아니라 구성 일관성**일 가능성이 높다. 그렇게 이해하는 편이 안전하다.

재현 잔여물은 스크래치 디렉터리에만 생성했고 저장소에는 남기지 않았다 (`git status` 로 `*_wpftmp*` 부재 확인).

---

## 4. 미검증 (Gaps)

- **변경 전 PE machine 값 미확보** — §3. 사후 재현 2회 모두 실패.
- **Release 구성 미빌드.** Debug 만 확인.
- **`gui/ImageProcTest.slnx` 재빌드 안 함.** gui 측 csproj 를 손대지 않았으므로 영향이 없다고 판단해 생략했다. 검증하지 않았으므로 "영향 없음"은 추론이지 관측이 아니다.
- **WPF 앱 미실행.** 빌드·테스트만 확인했다. x64 고정 후 앱이 정상 기동하는지는 확인하지 않았다.
- **Visual Studio IDE 미확인.** 이제 솔루션 내 두 프로젝트가 모두 x64 이므로 GUI-C-03 에서 우려했던 비대칭은 해소되었으나, IDE 구성 드롭다운 실제 표시는 확인하지 못했다.
- **90건 중 네이티브 부재로 실질 무검증인 항목 분류 안 함** (GUI-C-03 에서와 동일, 이번에도 `xpe_common.dll` 부재 상태).

## 5. 잔여 위험

- 이 프로젝트는 이제 x64 전용이다. ARM64 Windows 등 다른 아키텍처 대상 빌드가 향후 필요해지면 `Platforms` 를 확장해야 한다. 현재 XPE 는 x64 네이티브 DLL 에 P/Invoke 하므로 실질 제약은 아니다.
- §3 결론대로 이 변경의 실익이 구성 일관성에 그친다면, 향후 누군가 "왜 넣었나"를 물을 수 있다. 커밋 본문에 그 한계를 명시했다.

---

## 부록 — 사용한 명령

```bash
export PATH="/c/Program Files/dotnet:$PATH"
sed -i 's|...<TargetFramework>...|...+ Platforms/PlatformTarget|' clients/ImageProcTest/ImageProcTest.csproj
rm -rf clients/ImageProcTest/bin clients/ImageProcTest/obj
dotnet build clients/ImageProcTest.slnx -c Debug              # exit=0, 경고 0
dotnet test  clients/ImageProcTest.slnx -c Debug --no-build   # 90/90
find clients/ImageProcTest/bin -maxdepth 2 -type d            # x64 하위폴더 부재 확인
# PE machine: od -An -tu4 -j60 -N4 <dll> 로 PE 헤더 오프셋, +4 위치 2바이트 판독
git status --short                                            # M csproj 1건만
```
