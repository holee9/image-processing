ACK #38 2026-10-01 17:00:02 UTC

# Codex reviewer #38 — QA-A-202d

인박스 SHA-256 `09426bd121732af6b5d420d90cc23751d3774c4e857b398a48eee8c113be1f9f` 확인. `1e6ecd07..f2ca59d7` 모듈 diff, QA-A-202d 보고서, #32의 네 발견, 관련 호출 경로와 로컬 spdlog 1.14 구현을 검토했다. 새 빌드·실행 시험은 하지 않았다. #34의 QA-A-205b 보류는 별도 범위다.

## 발견

### 1 — 높음, 차단: 품질 필드 없는 새 게인 맵은 이전 품질 기록과 함께 확정됨

- **무엇:** 리더의 `gain_quality` 단일 출처 결정에서 벗어나 `g_calib.quality_meta`를 공용 조회 기록으로 둔 이유는 타당하다. 생성 기록의 이력과 파일 원본 품질은 다른 값이다. 하지만 `xpe_calib_commit_gain_locked` (`xpe_calib_load_gain.cpp:176-181`)은 새 맵·`gain_has_quality=false`를 설치하면서 `staged.qualityFound`가 참일 때만 `quality_meta`를 바꾼다. `calibration_cache.cpp:509-521`의 캐시 적중 설치도 품질이 없으면 기록을 그대로 둔다. 품질이 없는 게인 파일은 허용되므로 맵 B와 파일 A의 품질 기록이 한 잠금 안에서 *함께* 확정된다. 데이터 경쟁은 해소됐지만 #32 발견 1의 ‘새 맵 + 옛 품질’ 상태와 `preprocess_api.h:846-849,1599-1602` 및 verdict 4절의 무조건적인 계약은 여전히 성립하지 않는다.
- **재현:** 품질 메타데이터가 있는 게인 A를 적재하고 `xpe_calib_get_quality_meta`로 A를 확인한다. 품질 키가 없는 정상 게인 B를 파이프라인 세트 경로 또는 캐시 적중으로 적재한다. B 맵이 사용되는데 같은 조회는 여전히 A의 `r_squared`를 반환한다. 스레드 경쟁 없이도 재현된다.
- **고침:** 맵 확정 시 품질 부재도 명시적으로 기록한다(유효성/출처 표시와 안전한 기본값). 또는 공용 API를 ‘현재 맵의 품질’이 아니라 ‘마지막으로 알려진 품질 이력’으로 정확히 정의하고, 현 맵과 품질의 연관성을 확인할 식별자·유효성 필드를 별도로 제공한다. 세트 적재와 캐시 적중의 품질 있음/없음 행렬 시험을 추가한 뒤 리더에게 계약 변경을 확정받는다.

### 2 — 보통: 캐시 적중이 파일 품질 사본을 현재 맵과 동기화하지 않음

- **무엇:** 이 설계의 `g_calib.gain_quality`는 현재 게인 파일의 원본 품질이며 캐시 게시 시 읽힌다(`calibration_cache.cpp:680-681`). 그런데 캐시 적중의 `install_gain` (`:502-521`)은 맵·치수·시각·세션과 공용 `quality_meta`만 갱신하고 `gain_quality`·`gain_has_quality`는 갱신하지 않는다. 따라서 A를 적재한 뒤 B 캐시 적중을 설치하면 현재 맵 B와 파일 품질 사본 A가 공존한다. 이번 변경 전에도 존재한 누락이지만, 두 품질 저장소를 구분해야 한다는 이번 설계의 불변식을 깨뜨린다.
- **재현:** 서로 다른 품질값의 A/B를 캐시에 채운 뒤 A를 현재로 만들고 B를 캐시 적중시킨다. 잠금 아래 `g_calib.gain_map`이 B이고 `g_calib.gain_quality`가 A인지 확인한다. 이어지는 캐시 게시/진단이 이 필드를 사용하면 잘못된 메타데이터를 전파할 수 있다.
- **고침:** 적중 메타데이터의 `quality`와 `hasQuality`를 맵·치수와 같은 임계 구역에서 `gain_quality`·`gain_has_quality`에도 설치한다. 캐시 항목의 품질 없음도 false로 덮는다.

### 3 — 보통, 시험 신뢰도: 읽기 잠금 시험은 지연된 스레드를 대기로 오인할 수 있음

- **무엇:** `test_oom_injection.cpp:1484-1491`은 읽기 스레드가 `g_started=true`를 쓴 뒤 실제 getter의 mutex에 도달했는지 알지 못한 채 150 ms 후 `!g_done`을 검사한다. 스레드가 신호 직후 선점되면 잠금 없는 잘못된 구현도 ‘대기 중’으로 통과할 수 있다. 부하가 큰 CI에서 150 ms 스케줄링 지연은 실패 증거가 아니다. 완료 후 B 값을 확인하는 단언도 읽기가 commit 뒤에 시작됐다면 통과한다.
- **재현:** 시험용 reader에서 `g_started` 직후 200 ms를 지연시키고 getter의 잠금을 제거한다. 기존 두 단언이 모두 통과하는지 확인한다.
- **고침:** getter의 잠금 획득 전·후에 시험용 훅/장벽을 두고 실제로 획득 시도 지점까지 도달했음을 동기화한다. 또는 ThreadSanitizer/동시성 계측을 보조 증거로 사용하고 이 시간 시험의 증명 범위를 좁힌다.

## #32 항목 및 추가 확인

- **① 품질:** `g_quality_meta`의 잠금 밖 읽기/쓰기는 사라졌고 생성기·getter·게인 확정·캐시 품질 갱신 모두 `g_calib_mutex`를 쓴다. 서로 다른 두 품질 출처를 유지하는 설계는 합리적이나, 발견 1·2의 품질 부재/캐시 상태를 고치기 전에는 무조건적인 맵-품질 원자 계약을 승인할 수 없다.
- **② 스냅샷:** `CalibSnapshot`은 세 맵의 `shared_ptr`, 치수, 다항식 범위, 초기화 상태를 한 잠금으로 복사한다. offset/gain/defect `_in`은 이 인자를 쓰며, 세 파이프라인 진입점과 배치는 한 스냅샷을 `pipeline_core`에 넘긴다. 경로 적재는 세트 확정과 스냅샷을 같은 잠금에서 수행한다. 캐시 적중 설치가 겹쳐도 잠금 순서에 따라 프레임은 한 버전의 맵을 계속 소유한다. 검토한 세 보정 본문에서 `g_calib` 재조회는 찾지 못했다. 비선형성 표는 계약상 제외된다.
- **③ 로거:** 현재 `xpe_log_set_file`은 이전 싱크를 설치 전에 flush하고 새 싱크/로거를 만든 뒤 기본 로거를 바꾼다. spdlog 1.14 `base_sink<Mutex>::flush()`는 싱크 mutex를 잡으므로 `basic_file_sink_mt`의 쓰기와 직접 flush는 동기화된다. `set_default_logger`의 레지스트리 삽입이 실패하면 기본 포인터를 바꾸기 전이며, 이후 `shared_ptr` 이동/해제는 예외를 던지지 않는다. 단, spdlog 1.14 `registry-inl.h:92-105`는 *같은 레지스트리*에서 `spdlog::info()`와 `set_default_logger()`를 동시에 호출하지 말라고 명시한다. `g_logMutex`는 외부 직접 호출을 보호하지 않는다. 현재 빌드의 `SPDLOG_BUILD_SHARED=OFF`에서는 다른 DLL의 직접 호출이 같은 레지스트리를 쓰는지 별도로 확인해야 하므로, 보고서의 ‘전환 중 직접 로그는 A 또는 B’라는 설명을 스레드 안전성 증거로 사용할 수 없다.
- **④ append:** `basic_file_sink_mt(filePath, false)`로 같은 파일 재지정의 truncate는 제거됐다. 이전 싱크를 먼저 flush하고 새 파일을 append로 열므로 #32의 기존 행 손실 경로는 해소됐다.
- **연결:** `xpe_common.cpp`의 `extern "C"` 블록은 `xpe_alert_push`뿐이다. `xpe_logging.cpp` 블록의 변경 대상은 수출 함수이며 `xpe_log_set_file`에는 try가 있다. 세 보정 `_in`과 `CalibSnapshot`은 내부 헤더의 C 블록 밖에 있고, 수출 wrapper 셋은 각각 try/catch로 스냅샷 생성과 `_in` 호출을 감싼다. DLL/OOM 시험 오브젝트의 `/EHsc /Zi /O2 /Ob1 /arch:AVX2 /W4 /WX` 플래그는 보고된 `build.ninja` 발췌와 일치한다.

판정: 보류
