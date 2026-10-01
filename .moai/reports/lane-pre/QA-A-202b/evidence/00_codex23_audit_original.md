ACK #23 2026-10-01 14:57:33 UTC

# 독립 검토 #23

대상 인박스 SHA-256: `7841f4dfbfb0ca3410f8fe11bcb3470757a80127454d61e0dad31fc30dd3d722` 확인. 지정된 두 커밋 구간의 `modules/` 차이와 QA-A-202/203b 보고서·시험 증거를 검토했다. 현재 작업 트리의 별도 미커밋 변경은 판정 범위에 넣지 않았다.

## 발견 1 — 높음: 세 파이프라인 C ABI 진입점에 할당 예외가 남음

- **무엇:** `pipeline.cpp:333-338,436-441,464-471`의 `xpe_preprocess_pipeline`, `_ex`, `_batch`는 함수 전체 예외 가드가 없다. 세 함수가 모두 호출하는 `PipelineConfig::fromJson`은 `xpe_json_get_string`이 반환하는 `std::string`을 여러 번 생성한다(`pipeline.cpp:52-95`, `helpers.cpp:60-92`). `pipeline_core`도 여러 `std::vector::resize`로 프레임 버퍼를 할당한다(`pipeline.cpp:134-288`). `std::bad_alloc`이 나면 여전히 수출 C ABI 밖으로 전파된다. 엄격한 숫자 변환은 잘못된 숫자의 `stof/stoi` 예외만 없앤다.
- **재현:** 정적 OOM 시험 실행 파일에서 긴 설정 필드를 주고 문자열 생성 시점의 `operator new`를 실패시키거나, 유효한 설정으로 첫 단계의 `stage1Data.resize`를 실패시켜 세 수출 함수를 각각 호출한다. 현재는 `XPE_ERR_OUT_OF_MEMORY` 대신 `std::bad_alloc`이 탈출한다. MSVC `/EHsc`에서 이 경계를 안전하다고 볼 수 없다.
- **수정 방향:** 세 진입점의 본문 전체를 함수 try 블록 또는 최외곽 `try`로 감싸 `bad_alloc`→`XPE_ERR_OUT_OF_MEMORY`, 그 밖의 예외→`XPE_ERR_PROCESSING_FAILED`로 변환한다. 실패 전후 이미지·메타데이터·교정 저장소 상태를 OOM 주입 시험으로 확인한다. 설정 파싱과 단계 버퍼 할당을 모두 주입 범위에 넣는다.

## 발견 2 — 중간: 이전에 허용된 설정 숫자 표기가 거부됨

- **무엇:** 새 `xpe_strict_parse.hpp:8-9,24-52`는 앞 공백과 `+`를 명시적으로 거부한다. 기존 `std::stof/stod/stoi`는 이를 허용했다. 따라서 예를 들어 `{"detectorTempC":" +25"}`, `{"binningMode":"+2"}`, `{"alpha1":" 0.5"}`가 이전에는 처리됐지만 이제 `XPE_ERR_CONFIG_INVALID`다. 소수의 지수 표기 `1e2`는 `from_chars`에서 계속 유효하지만, 정수 필드 `"1e2"`는 예전 `stoi`가 앞의 `1`로 처리하다가 이제 전체 소비 검사로 거부된다. 뒤 글자 거부는 이번 엄격 변환 의도와 부합한다. 앞 공백·앞 `+`의 거부는 정상 숫자 구성의 호환성 변경이다.
- **재현:** 위 설정을 유효한 영상/메타데이터와 함께 세 파이프라인 진입점 및 `xpe_ghost_create`에 각각 전달하고 이전 기준 커밋과 결과를 비교한다.
- **수정 방향:** 기존에 정상으로 취급한 앞 공백·앞 `+`를 유지할지 계약으로 결정한다. 유지한다면 비할당 정규화 후 전체 숫자를 엄격하게 파싱한다. 의도적 거부라면 공개 API 문서·호환성 안내에 명시하고 경계 시험을 추가한다. 정수 필드의 뒤 글자·지수는 엄격화 의도대로 거부할 수 있다.

## 확인한 범위

- 숫자 변환 대상 9필드(`detectorTempC`, `binningMode`, `tier`, `alpha1`, `tau1`, `alpha2`, `tau2`, `tier2Threshold`, `nlcscBeta`)는 모두 `from_chars` 헬퍼를 사용한다. `xpe_calib_mode.cpp`의 `atoi/atof` 네 곳은 남아 있지만 예외를 던지는 함수는 아니며 이번 1·2순위의 변경 대상은 아니다.
- `xpe_offset_correct`와 `xpe_defect_correct`의 함수 try 블록은 본문 전체를 덮는다. 잠금 아래에 `shared_ptr` 소유권을 확보하고 잠금 밖에서 읽으므로 동시 재적재 시 해당 호출의 이전 맵 수명이 유지된다. 정적 결함 병합은 새 맵을 구성한 뒤 잠금 아래에서 교체한다. `offset_correct`는 현재 프레임당 할당이 없어 예외 가드는 방어적이며, 그 자체의 OOM 반증은 없다.
- 설정을 교정 파일보다 먼저 읽으므로 설정 오류와 교정 파일 오류가 동시에 있을 때 이제 `XPE_ERR_CONFIG_INVALID`가 먼저 나온다. 이는 거부된 설정의 적재 부작용을 막는 의도적 순서 변경이다.
- QA-A-203b는 `get_copy`의 두 번째 조회에서 파일 열기 확인 뒤 캐시 잠금 안에 `now_epoch_ms()`를 호출하고, 화소 복사 전에 만료를 판정한다. `XPE_CACHE_TEST_HOOKS`는 OOM 시험 타깃에만 정의된다. 증거에서 정상 OOM 18/18, `clockearly` 반증 17/18, `noindex` 반증 11/18이며, 색인 누락을 일관성 단언이 검출한다. QA-A-202의 일반 시험 760 통과/8 건너뜀, OOM 17/17 및 `guard`·`unowned` 반증 로그도 확인했다. 시험을 별도로 재실행하지는 않았다.

판정: 보류 — 발견 1의 C ABI 예외 탈출을 닫고 발견 2의 호환성 결정을 명시한 뒤 재검토.
