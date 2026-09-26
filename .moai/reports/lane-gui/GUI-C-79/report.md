# GUI-C-79 보고 — #172 바인딩 복원, #171 통제 ①②③ + 결함 주입

커밋(dev/gui, 미푸시): 3831b48 (#172 복원·W-20), 8eb1e28 (② W-21), 19f0a18 (① W-22), d3253d6 (결함 주입·W-24), 81dbaf5 (③ W-23), 1ac7802 (소비 검사 제외 목록)

## 1. 주장

1. 메인 뷰포트가 `SourceImage`/`ProcessedImage` 를 받는다 (W-20). `ComparisonViewportDetected` 는 상수에서 실측(뷰포트 두 DP 비null)으로 바뀌었다.
2. HUD 는 영상을 만든 VOI 를 표시한다 (W-21).
3. 표시 입력을 바꾸고 적용하지 않으면 오래됨 표시가 뜬다 (W-22).
4. 표시 파이프라인이 실패하면 이전 영상이 남고 오래됨 표시가 뜬다 (W-23).
5. 결함 주입은 명령줄 전용이며 인자 없는 실행에서는 생성되지 않는다 (W-24, AutomationArgsTests).
6. 각 통제를 끄면 그 시험만 빨개진다.

## 2. 증거 (이 디렉터리)

- `red.txt` — ③ 구현 전 W-23 실패(`indicator=''`, v3 유지, `calls=3`), W-24 통과
- `green.txt` — ③ 구현 후 W-20~24 통과 5/5
- `falsify3.txt` — ③ 한 줄 끔: 통과 4 / 실패 1 (W-23)
- `falsify1.txt` — ① 호출 끔: 통과 4 / 실패 1 (W-22)
- `full-e2e.txt` — Mock 전체: 실패 0 / 통과 76 / 건너뜀 1
- `full-int.txt` — 통합: 실패 0 / 통과 209 / 건너뜀 1
- `native.txt`, `native-run-1.trx` — Native 1회: 실패 0, 통과 76, NotExecuted 1 (WindowReacquire), NativeProvenance 통과
- 자동화 보고(Mock, CI 와 같은 인자): `exit=0 Passed=True ComparisonViewport=True`
- ②·① 반증과 #172 반증은 이전 단계 커밋 메시지와 #172/#171 코멘트에 기록
- 이슈: #172 https://github.com/holee9/image-processing/issues/172#issuecomment-5706818327 , #171 https://github.com/holee9/image-processing/issues/171#issuecomment-5706942247

## 3. 기준선 귀속

모든 수치는 이 세션, dev/gui 1ac7802 트리(반증은 해당 한 줄만 바꾼 작업본), 빌드 `dotnet build clients/ImageProcTest.slnx` 경고 0 / 오류 0 에서 측정. Native 는 기존 `build/ci-common/bin` DLL 사용(재빌드 없음).

## 4. 미검증

- 분리 뷰어: 소스 판독만. `ProcessedImage` 는 받지만 오래됨 배너·HUD 가 없어 ①③ 표시가 닿지 않는다. 실행 관측은 하지 않았고 수정하지 않았다.
- 전처리 거부(`PREPROCESS_NOT_RUN`)는 ③ 범위에 넣지 않았다.
- 결함 주입 반증(“Wrap 이 인자 없이도 감싸면 W-24 가 빨개지는가”)은 실행하지 않았다. W-24 의 대조는 같은 속성이 W-23 무장 앱에서 다른 값을 돌려준다는 관측이다.
- Native 에서 W-23 은 결함 주입이 Native 백엔드를 감싼 상태로 통과했으나, Native 자체의 파이프라인 실패(주입 아님)는 재현하지 않았다.
- CI 실행 결과는 아직 없다 (push 는 lead 몫).

## 5. 잔여 위험

- UI 시험은 `Thread.Sleep` 폴링 기반이라 느린 러너에서 타이밍 흔들림 가능.
- 무장 시험 앱은 호출 수에 의존한다. 로드·프리셋 경로가 파이프라인을 더/덜 호출하게 바뀌면 W-23 전제(`calls=1` 시작)가 먼저 실패한다 — 조용히 통과하지는 않는다.
- `FaultInjectingBackend.Armed` 는 프로세스 정적 값이며 팩토리가 두 번 호출되어 마지막 래퍼를 가리킨다.
- `AutomationRunSelectionConsumptionTests` 제외 목록 추가는 원칙상 판정 대상이다.

## Card Cross-Check

| 항목 | 카드 |
|---|---|
| 분리 뷰어에 ①③ 표시 없음 | 신규 카드 후보 (lead 판단) |
| 2-lane | #173 (이 카드 아님) |
