# GUI-C-118 — 설정이 조용히 사라지는 경로, 측정 (#173)

카드대로 **고치지 않았다.** 측정만 했다.

## 1. 주장

세 가지 방식 모두에서:

1. **전부 사라진다.** 같은 파일 안의 멀쩡한 키까지 같이 없어진다. 일부 생존은 없다.
2. **화면에 아무것도 안 나타난다.** 호출자가 알 방법 자체가 없다.
3. **다음 저장이 원본을 덮어쓴다.** 백업은 남지 않는다.
4. 즉 **되돌릴 수 없다.**

## 2. 증거

`CorruptSettingsFileTests` (IntegrationTests). 세 파일 모두 **멀쩡한 키 3개**(`voiWindowCenter: 1234`, `laneBAlgorithm: "Virtual grid"`, `comparisonZoomScale: 2.5`)를 담고 한 곳만 깨뜨렸다.

### 대조군 — 같은 값이 정상 파일에서는 읽힌다

```
control: center=1234 algorithm='Virtual grid' zoom=2.5
```

이 대조가 없으면 "그냥 아무것도 안 읽힌다" 와 구별되지 않는다.

### (a) 무엇이 사라지는가 — 전부

```
[truncated]  survived: center=False algorithm=False; everything at defaults=True
             loaded: center=32768 algorithm='Grid suppression' zoom=0
[wrong-type] survived: center=False algorithm=False; everything at defaults=True
             loaded: center=32768 algorithm='Grid suppression' zoom=0
[utf-16]     survived: center=False algorithm=False; everything at defaults=True
             loaded: center=32768 algorithm='Grid suppression' zoom=0
```

**`wrong-type` 가 가장 분명하다.** 깨진 키는 `voiWindowCenter` 하나(`"not a number"`)인데, 그 뒤의 `laneBAlgorithm`·`comparisonZoomScale` 은 문법도 타입도 멀쩡하다. 그런데도 **셋 다 사라졌다.** `JsonSerializer.Deserialize` 가 첫 실패에서 예외를 던지고, `Load` 의 `catch` 가 그것을 통째로 삼켜 새 `AppSettings` 를 돌려주기 때문이다.

깨뜨린 방식은 서로 다른 현실을 흉내 낸 것이다 — 잘린 JSON(저장 중 종료·디스크 가득), 잘못된 타입(손편집·버전 간 타입 변경), UTF-16(윈도우 도구가 "유니코드" 로 저장).

### (b) 화면 — 아무것도 없다

```
Load returns: AppSettings only — no status, no exception, no flag.
A corrupt file and an absent file produce the same object.
```

코드에서 확인한 것:

- `AppSettingsService.Load` 는 `catch { return new AppSettings(); }` — 예외를 삼키고 **반환 타입에 실패를 담을 자리가 없다**.
- `MainWindow.xaml.cs:112` 의 호출부는 `shipped.Load()` 한 줄이고 그 결과를 검사하지 않는다(검사할 것이 없다).
- 이 호출은 **뷰모델이 만들어지기 전**에 일어나므로, 알림을 띄울 대상(`Alerts`·`StatusText`)이 아직 없다.

**즉 "조용함" 의 크기는 최대치다.** 사용자가 보는 것은 *"설정이 사라졌다"* 가 아니라 *"내가 뭘 잘못 눌렀나"* 이다.

### (c) 다음 저장 — 원본이 사라진다

```
[truncated]  after Save: original bytes preserved=False; 89 bytes  -> 1482 bytes
[wrong-type] after Save: original bytes preserved=False; 105 bytes -> 1482 bytes
[utf-16]     after Save: original bytes preserved=False; 190 bytes -> 1482 bytes
backup alongside: (원본 경로 하나뿐 — .bak 등 없음)
```

`Save` 는 `File.WriteAllText` 한 줄이라 **깨진 파일이 정상적인 기본값 파일로 대체된다.** 덮어쓰고 나면 원본에 무엇이 들어 있었는지 알 방법이 없다 — 되돌릴 수 없다.

**다만 저장은 자동이 아니다.** `_settingsService.Save` 호출부는 코드 전체에 1곳(`SaveSettings`)이고, 그것은 File ▸ Save 메뉴/버튼 명령이다. 창을 닫을 때(`OnClosed`)는 저장하지 않는다. 즉 덮어쓰기는 **사용자가 저장을 누를 때** 일어난다 — 기본값으로 돌아간 화면을 보고 설정을 고친 뒤 저장하는 것이 가장 자연스러운 행동이므로, 실제로는 도달하기 쉬운 경로다.

## 3. baseline 귀속

```
BUILD_EXIT=0
Mock  E2E 전체   : 통과 113, 건너뜀 22, 실패 0 (5m)
IntegrationTests : 통과 252, 건너뜀 1, 실패 0  (측정 케이스 4건 추가)
```

직전 실행 대비 Mock 이 114/21 → 113/22 로 바뀌었다. 늘어난 건너뜀 1건은 `WindowReacquireTests.ReadableWindow_IsKept_AndLeavesNoNote` 로, **GUI-C-74 에서 런치당 약 7% 로 측정해 GUI-C-75 가 실패 대신 건너뜀으로 회계 처리하기로 한 그 항목**이다. 이 카드의 변경과 무관하다(이 카드는 IntegrationTests 파일 하나만 추가했다).

## 4. 미검증 (Gaps)

- **실행 중인 앱에서는 재지 않았다.** 측정은 `AppSettingsService` 를 직접 호출한 것이다. 화면에 아무것도 안 나온다는 결론은 코드 경로를 읽어 뒷받침했을 뿐, E2E 로 깨진 파일을 놓고 앱을 띄워 보지는 않았다.
- **배포 경로의 파일 위치**(`AppContext.BaseDirectory\appsettings.json`)에서의 권한·동시 쓰기 문제는 보지 않았다.
- 깨뜨리는 방식 셋은 **대표 사례**이지 전수가 아니다. 예를 들어 빈 파일·BOM 만 있는 파일·JSON 배열 루트는 재지 않았다.
- `File.WriteAllText` 가 쓰기 도중 죽으면 어떻게 되는지(부분 기록)는 재지 않았다 — 그것이 애초에 `truncated` 를 만드는 경로일 수 있다.

## 5. 잔여 위험

- 측정이 맞더라도 **빈도는 모른다.** 이 경로가 실제 사용자에게 얼마나 자주 일어나는지에 대한 증거는 없다.
- 고치는 방향에 따라 새 위험이 생긴다 — 예컨대 백업을 남기면 그 백업이 언제 지워지는지가 새 질문이 된다. 방향 결정은 리더 몫이다.

🗿 MoAI
