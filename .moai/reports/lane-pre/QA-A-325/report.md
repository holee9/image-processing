[2026-10-29 16:21 KST] [xpe-leader 대행 QA-A-325]

# QA-A-325 — 출하 근접 설정 s6 15 ms 이하 (KR6, Refs #278)

> 하네스가 작업자의 보고서 쓰기를 막아 리더가 요점을 옮겼다. 원문은 evidence/(증거 커밋 026ab7ff, 방법 00_method.md). 소스 커밋 a793ff24.

## 결과 (포화 켬 + 줄 보간 끔, 회차별 중앙값의 중앙값 ms, 확인 측정 8회, 25_)
| 세트 | A(HEAD) | B(수정) | B 회차 최대 |
|---|---|---|---|
| s0_gf | 12.01 | 11.68 | 12.19 |
| s1_um300 | 13.46 | 13.10 | 13.54 |
| s6_gad_pre | 15.36 | 14.67 | 14.94 |
- 재현(02_, 03_, 6회): s6 15.22(14.94~15.53). 같은 A 가 측정마다 15.22~15.74 로 흔들림.
- 출력 sha 18/18 동일(QA-A-315 15개 + sat_on_interp_off 3개). B16 점수·보정 json 이 QA-A-324 와 바이트 동일, 표 diff 없음(42_, 44_).
- 시험: 필터 213 통과·3 건너뜀(SaturationRealData, 환경변수 미설정), oom 99/99, ParallelUnwind 1/1.

## 단계 분해 (s6, 수정 전, 11_)
이득 1.99, 포화 적용 0.85, 히스토그램 0.28, 결함 단계 9.43(채움 4.64·거리표 2.24), 복사 0.69+0.89. s6 이 느린 이유는 결함 채움(s1 3.13, s0 1.23) — 고리 반지름 분포 차이는 추정.

## 수정 (a793ff24)
xpe_sat_apply 를 프레임 전체 별도 패스 대신 이득 단계가 쓴 8192 화소 블록에 바로 적용(gain_correct.cpp applyBlock). apply_saturation → prepare_saturation(이득 전, 수준·S_out·알림) + monitor_saturation(이득 성공 후 히스토그램). xpe_gain_correct_in 에 선택 인자 추가, 공개 API 불변. 이득+포화 2.84 → 2.33 ms.

## 미확인
- s6 여유 0.06~0.33 ms — 다른 날·부하에서 15 ms 를 넘을 수 있음. 추가 후보: 히스토그램 병렬화(0.28~0.46), 결함 마스크·목록·거리표 프레임 간 캐시(약 4 ms).
- SaturationRealData 3개, 전체 ctest, 다른 기계, s4·s5·s7, 180장 전체.

## 리더 판정 (2026-10-29 16:24)
- diff 직접 확인: pipeline.cpp:553-559 — 포화 적용 위치만 이득 단계 안으로 옮겼고, 이득 실패 시 반환(:556) 뒤 monitor 를 부르지 않는 순서가 전과 같다. raw = img->data(:222)이고 넓힘 버퍼는 별도 할당 work.get()(:450)이라 겹치지 않는다. 인덱스 61 유지.
- 판정: 수용. KR6 은 출하 근접 설정에서 세 세트 모두 15 ms 이하 달성, 단 s6 여유가 작다는 점을 함께 공개한다. push 후 CI 와 Reviewer 검토, main 병합.

## F325-1 보완 (Reviewer 16:25 P2) — [2026-10-29 18:21 KST] 리더 기록

- 결함: a793ff24 에서 prepare_saturation(이득 평면 중앙값, 할당+전체 읽기)이 xpe_gain_correct_in 의 거부 검사보다 앞섰다.
- 수정 6ce56a2d: xpe_gain_correct_in 이 XpeGainSatHook 을 받아 마지막 거부 검사(gain_correct.cpp:519) 뒤, 화소 쓰기 전(:525)에 한 번 호출. 감시는 규칙이 적용된 경우에만(pipeline.cpp:590).
- 재현 시험 OomPipeline.AFrameTheGainStageRefusesIsRefusedBeforeTheSaturationStepAllocates: 수정 전 할당 off 21 / on 22 실패(f1_10), 수정 후 통과(f1_11). 대조군 2개(맵 없음 = 같음, 정상 프레임 = on 이 많음).
- 시험: 216 실행 213 PASS 3 SKIP(SaturationRealData), oom 100/100, ParallelUnwind 1/1. 출력 sha256 18/18 동일(f1_21), B16 json 2개 바이트 동일.
- s6 교대 8라운드: 수정 중앙값 14.933 ms, 같은 날 HEAD 재빌드 14.843 ms, 짝 차이 중앙값 +0.033 ms(양수 4/8) — 회귀 없음. 두 변형 모두 일부 라운드 15 ms 초과(오늘 기계 상태에서 HEAD 도 14.84 ms).
- 증거 2135a9a6 (f1_*). 임시 폴더 xpe-data/qa-325-f1-tmp, 스크래치 fixsrc 삭제.

### 리더 확인
- git show 6ce56a2d 로 diff 직접 읽음. 훅 호출 위치(:525) 뒤에 남은 반환은 catch 의 OUT_OF_MEMORY 뿐이며 훅의 bad_alloc 도 같은 경로. 훅을 넘기는 조건(gain_map && UINT16)은 수정 전 satFused 조건과 같고 알림 순서도 같다.
- 판정: 받아들인다. push 후 CI 와 Reviewer 재검토를 거쳐 main 병합.
- 주의: KR6 는 "지정 조건 측정 실적"이며 오늘 측정에서 HEAD 도 중앙값 14.84 ms, 일부 라운드 15 ms 초과 — 여유가 작다는 기존 공개 사항을 유지한다.
