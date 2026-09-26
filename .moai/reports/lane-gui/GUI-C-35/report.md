# GUI-C-35 — Body Part 선택 컨트롤 + W-07 + C-34 반증 보충 (#10 #136)

- 카드: GUI-C-35 · Refs #10 #136 · 워크트리 `D:/workspace-github/xpe-gui` / `dev/gui`
- 커밋 2건: `b373eeb`(컨트롤), `cacfc3d`(W-07 + 반증 2) — 미푸시
- **결과: E2E 9/9 · 통합 0/170/1/171 · slnx 0/0 · 자동화 Mock `Passed=true`**

---

## 1. 커밋 1 — Body Part 선택 컨트롤 (`b373eeb`)

C-34 실측: `ApplyBodyPartPresetCommand` 를 구동하는 UI 가 없어 프리셋은 자동화 하네스 코드에서만 닿았다. leader 결정으로 컨트롤을 추가했다.

| 결정 | 이유 |
|---|---|
| **툴바**에 배치 (Analysis 패널 아님) | 그 패널 내용은 탭 안에 있고, WPF 는 탭이 선택되기 전까지 자식을 UIA 트리에 넣지 않는다 — C-34 에서 `LogListBox`·VOI 입력이 실제로 그렇게 안 보였다 |
| 선택 = 적용 (별도 버튼 없음) | `SelectedBodyPart` 세터가 설정을 쓰고 커맨드를 실행한다. 버튼을 두면 "골랐는데 적용은 안 된 상태" 가 생긴다 |
| 옵션은 **기존** `BodyPartOptions`(`Enum.GetNames`) 재사용 | 두 번째 목록을 만들면 열거형과 갈라진다. 처음에 새 배열을 선언했다가 `CS0102`(중복 정의)로 발견해 제거했다 |

AutomationId: `BodyPartSelector` (앱 규칙).

## 2. 커밋 2 — W-07 (`cacfc3d`)

```
W-07 elapsed 2 409 ms  →  통과
```

툴바에서 `Lung` 을 고르면 상태바의 VOI 창이 `C=25000, W=50000` 으로 바뀐다(Mock 프리셋, `MockXpeBackend.cs:300`).

**값을 상수로 박지 않고 "활성 백엔드의 값" 으로 다뤘다.** C-23 이 Mock(25000/50000)과 네이티브가 다름을 실측했고, 한쪽을 박으면 "프리셋이 적용됐는가" 가 아니라 "어느 백엔드가 도는가" 를 재게 된다. 이 실행은 Mock 이므로 Mock 값을 기대하며, 주석에 그 근거를 남겼다.

상태바를 읽는 이유도 §1 과 같다 — Analysis 탭 안 요소는 탭 선택 전까지 존재하지 않는다.

## 3. 반증 2건

### ① 프리셋 값 변경 (`step3-falsify-a.log`)

Mock 의 Lung 중심값을 `25000 → 11111` 로 바꿨다:

```
실패!  - 실패: 1, 통과: 3, 전체: 4
W07_SelectingBodyPart_AppliesThatPresetToTheVoiWindow [FAIL]
```

W-07 만 실패한다. 원복 후 재확인.

### ② C-34 누락분 — **결과가 내 서술과 달랐고, 정정한다** (`step3-falsify-b.log`)

C-34 보고서에 이렇게 적었다: *"먼저 떼어내지 않으면 시작 시 자동 로드가 남긴 값으로 통과한다."*

실제로 재초기화 줄을 빼고 돌리니:

```
실패!  - 실패: 1, 통과: 0, 전체: 1
Assert.NotEqual() Failure: Strings are equal
```

**통과가 아니라 실패한다.** 상태바가 이미 파이프라인 요약이라 실행 후에도 같은 문자열이고, `Assert.NotEqual` 이 그것을 잡는다.

정정된 이해: 인과를 지키는 것은 재초기화 **단독**이 아니라 **재초기화 + `NotEqual` 쌍**이다. 둘 중 하나만 빠져도 시나리오는 유효한 관측이 아니게 된다 —

- 재초기화만 빼면 → `NotEqual` 이 실패시킨다(관측 가능한 고장).
- `NotEqual` 만 빼면 → 이력으로 통과한다(조용한 고장). ← C-34 서술이 실제로 가리키던 상황.

C-34 보고서의 문장은 **후자를 전자처럼 적은 것**이었고, 이번 실측으로 바로잡는다.

## 4. 실행 중 관측 — 유출된 앱 프로세스가 빌드를 잠갔다

최종 실측 도중 slnx 빌드가 실패하고 E2E 가 4건 무너졌다:

```
error MSB3027: … ImageProcTest.exe … 파일이 "ImageProcTest (25736)"에 의해 잠겨 있습니다.
error MSB3021: The process cannot access the file … because it is being used by another process.
```

앱 인스턴스 하나가 살아남아 exe 를 잡고 있었다. 프로세스를 정리하니 **9/9 로 복구**됐다.

`ApplicationFixture.Dispose` 는 `Close` 가 아니라 `Kill` 을 쓰는데도 새는 경로가 있다는 뜻이다(§6). **"테스트가 실패했다" 를 "코드가 틀렸다" 로 바로 읽지 않은 것이 이번 진단의 요점이다** — 실패 4건의 원인은 코드가 아니라 환경이었다.

## 5. 실측 (verbatim)

```
dotnet test clients/ImageProcTest.E2ETests/… -c Debug
통과!  - 실패: 0, 통과: 9, 건너뜀: 0, 전체: 9 (9.3 s)

  S-01 3 ms · S-02 126 ms · S-03 157 ms · S-04 416 ms · S-05 20 ms
  W-01 89 ms · W-01b 2 812 ms · W-02 875 ms · W-07 2 409 ms      (워크플로 합 6.2 s / 게이트 3 min)

ImageProcTest.exe --automation-backend Mock …
Passed=True · VoiPresetApplied=True · preset C/W=32768/65535

dotnet build clients/ImageProcTest.slnx -c Debug -t:Rebuild
    경고 0개
    오류 0개

dotnet test clients/ImageProcTest.IntegrationTests/… -c Debug
통과!  - 실패:     0, 통과:   170, 건너뜀:     1, 전체:   171
```

Baseline 귀속: E2E 8 → **9**(W-07 신규). 통합은 C-34 최종 `0/170/1/171` 과 동일 — gui UI·E2E 변경이라 통합 항목 불변. 자동화 리포트의 프리셋 값이 `32768/65535`(Abdomen 기본)인 것은 자동화 시나리오가 Body Part 를 바꾸지 않기 때문이며 C-30 이후와 동일하다 — 무회귀.

## 6. 미검증 (Gaps)

- **W-07 은 Lung 하나만 본다.** Bone/Head 는 확인하지 않았다.
- **Native 프리셋 값은 E2E 로 확인하지 않았다** — 카드가 Native 실행을 제외했다. C-23 이 자동화 리포트로 본 것이 전부다.
- **컨트롤의 시각적 배치를 확인하지 않았다.** 툴바가 좁을 때 잘리는지 등은 보지 않았다(C-30 과 같은 한계 — UIA 통과가 사람 눈에 보인다는 증거는 아니다).
- **프로세스 유출의 원인을 특정하지 못했다**(§4). 어느 실행이 남겼는지, `Kill` 이 실패했는지 아니면 호출되지 않았는지 확인하지 않았다.
- 반증 ②는 W-01b 한 건에만 적용했다. W-01·W-02 에 대한 반증은 이번에도 하지 않았다.

## 7. 잔여 위험 (Residual risk)

- **유출 프로세스가 다음 실행을 오염시킨다**(§4). 픽스처는 자기가 띄운 인스턴스만 죽이므로, 앞선 실행이 남긴 창을 다음 실행이 붙잡을 수 있다 — 이번에 관측된 4건 실패가 그 형태였을 가능성이 있다(특정하지 못함). 스위트 시작 시 잔존 인스턴스를 확인하는 장치가 없다.
- **W-07 이 상태바 문자열 형식(`C=…, W=…`)에 묶인다.** C-30 이후 이 스위트가 표시 문구에 의존하는 세 번째 지점이다.
- `SelectedBodyPart` 세터가 커맨드를 실행하므로, 설정 파일에서 값을 읽어 초기화하는 경로가 프리셋을 의도치 않게 적용할 수 있다 — 현재 초기화는 `Settings` 를 직접 채우므로 발생하지 않지만, 바인딩 경로가 바뀌면 조용히 달라진다.

## 부록 — 사용한 명령

```bash
export PATH="/c/Program Files/dotnet:$PATH"
dotnet test clients/ImageProcTest.E2ETests/… --filter "FullyQualifiedName~WorkflowScenarios" --logger "console;verbosity=detailed"
dotnet test clients/ImageProcTest.E2ETests/… --filter "FullyQualifiedName~W01b"      # 반증 ②
powershell -Command "Get-Process ImageProcTest | Stop-Process -Force"                # §4 정리
dotnet build clients/ImageProcTest.slnx -c Debug -t:Rebuild
dotnet test clients/ImageProcTest.IntegrationTests/… -c Debug
```
