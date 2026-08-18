# SPEC-XPE-P1A 품질 검증 보고서

**날짜**: 2026-04-16
**상태**: CRITICAL — 커밋 차단 (수정 필수)
**종합 점수**: 72/100 (경고 수준 초과)

## 요약

9개 SWU 구현 완료, 18개 내보내기 C API 함수, 943줄의 TDD 테스트. IEC 62304 Class B 문서 헤더 포함. 그러나 메모리 관리, 리소스 정리, 보안 경계 위반의 심각한 문제로 인해 커밋 승인 불가.

## TRUST 5 점수

| 차원 | 점수 | 상태 | 설명 |
|------|------|------|------|
| **Tested** | 65/100 | 경고 | 943줄 테스트; 오류 경로 및 메모리 누수 테스트 부족 |
| **Readable** | 82/100 | 통과 | 명확한 이름, Doxygen 헤더, @MX 태그 포함 |
| **Unified** | 78/100 | 경고 | malloc/delete 패턴 불일치, 파일 핸들 정리 문제 |
| **Secured** | 58/100 | **심각** | 버퍼 오버플로우 위험, 검증되지 않은 stride, CRC 우회 가능 |
| **Trackable** | 80/100 | 통과 | REQ 참조, IEC 헤더 존재, @MX 태그 형식 올바름 |

**최종 판정: CRITICAL — 커밋 금지**

---

## 심각한 문제 (커밋 전 필수 수정)

### 1. [심각] gain_correct.cpp:37-45 — 검증되지 않은 malloc + 도메인 전환

**파일**: `modules/preprocess/src/gain_correct.cpp`
**문제**: 메모리 할당 크기 검증 없음. `width * height`가 오버플로우되면 `n`이 작아져서 할당된 버퍼가 너무 작아짐 → 픽셀 쓰기 시 힙 버퍼 오버플로우.

```cpp
const size_t n = static_cast<size_t>(img->width) * img->height;  // 오버플로우 검사 없음
float* dst = static_cast<float*>(std::malloc(n * sizeof(float)));  // 크기 미검증
if (!dst) return XPE_ERR_OUT_OF_MEMORY;
for (size_t i = 0; i < n; ++i)
    dst[i] = static_cast<float>(u16[i]) * gain[i];  // 미검증 버퍼 쓰기
```

**수정안**: 할당 전 오버플로우 감지:
```cpp
const size_t max_pixels = (size_t)-1 / sizeof(float);
if (img->width > max_pixels / img->height) return XPE_ERR_INVALID_INPUT;

const size_t n = static_cast<size_t>(img->width) * img->height;
float* dst = static_cast<float*>(std::malloc(n * sizeof(float)));
```

**영향**: REQ-P1A-016 (도메인 전환 안전성)

---

### 2. [심각] calibration_manager.cpp — FILE* 모든 오류 경로에서 닫히지 않음

**파일**: `modules/preprocess/src/calibration_manager.cpp`
**위치 1** (calib_load_file, 46-89줄):
- 정상 경로는 78줄에서 닫음
- 하지만 CRC 계산(81줄)에서 예외 발생 시 파일 미닫음

**위치 2** (xpe_calib_save, 163-196줄):
- 쓰기 오류 경로에서 파일 닫음 ✓
- 하지만 로직 흐름이 불명확

**수정안**: RAII 래퍼 사용:
```cpp
FILE* f = std::fopen(filePath, "rb");
if (!f) return XPE_ERR_IO_FAILED;

// 범위 가드 사용
auto close_file = [](FILE* fp) { if (fp) std::fclose(fp); };
std::unique_ptr<FILE, decltype(close_file)> file(f, close_file);
```

**IEC 62304 영향**: Class B 안전 요구사항 — 지속 저장소 모듈의 리소스 누수

---

### 3. [심각] Ghost 핸들 magic 검증 불완전

**파일**: `modules/preprocess/src/ghost_correct.cpp`
**문제**: `xpe_ghost_destroy()` 이후 핸들 포인터 자체는 무효화되지 않음. 파괴된 메모리가 재할당되고 magic이 `0xA7057AC0u`와 일치하면 검증 실패.

**수정안**: 클라이언트 책임 문서화:
```cpp
/**
 * @brief Ghost 보정 핸들 해제
 * @param handle 해제할 핸들 (NULL 가능, no-op)
 * @note **중요**: 호출자는 이 함수 이후 handle을 NULL로 설정해야 함
 */
void xpe_ghost_destroy(void* handle);
```

**테스트 추가**: 파괴된 핸들 사용 시 오류 반환 확인

---

### 4. [높음] Stride 검증 부족 — 연속적이지 않은 이미지 버퍼 오버플로우

**파일**: `offset_correct.cpp:27`, `gain_correct.cpp:33` 등
**문제**: stride 검증 없음. stride > width * elementSize인 경우 행 경계 넘어 접근.

**수정안**: stride 검증 추가:
```cpp
const uint32_t expected_stride = img->width * sizeof(uint16_t);
if (img->stride != expected_stride) return XPE_ERR_INVALID_INPUT;
```

또는 API 문서화: "stride는 width * element_size와 동일해야 함 (연속 레이아웃 필수)"

---

## 높은 수준 문제 (권장)

### 5. [높음] gain_correct.cpp — 소유권 이전 문서화 부족

**파일**: xpe_preprocess_api.h:42 의 Doxygen 주석 부족
**문제**: API 헤더에서 img->pixels 할당이 새로운 float32 버퍼로 교체된다는 문서 없음. 호출자가 free()로 해제해야 한다는 정보 부족.

**수정안**: API 헤더 업데이트:
```cpp
/**
 * @param img [in/out] 보정할 이미지 (uint16 입력, float32 출력)
 *            **중요**: 이 함수 후 img->pixels 해제는 호출자 책임 (free() 사용)
 */
```

---

### 6. [높음] defect_detect_runtime — mean+3sigma 한계

**파일**: `defect_correct.cpp:45-75`
**문제**: 고정 3-sigma 임계값은 Poisson 잡음이나 비정규 분포에서 불안정. 설정 불가능.

**테스트 부족**: 균등 픽셀, 이봉 분포 엣지 케이스 미테스트

---

## 중간 수준 문제 (권장)

### 7. [중간] 오류 경로 테스트 커버리지 부족

**파일**: 모든 test_*.cpp
**부족한 항목**:
- malloc 실패 시뮬레이션
- 파일 I/O 오류 (권한 거부)
- CRC-32 불일치 시나리오
- Ghost 핸들 동시 접근 (REQ-P1A-066)

**추가할 테스트**:
```cpp
TEST_F(CalibManagerTest, CrcMismatchReturnsIoError) {
    // 파일 저장, payload 바이트 손상, 다시 로드 → XPE_ERR_IO_FAILED 예상
}
```

---

### 8. [중간] Include 가드 불일치

**파일**: `xpe_preprocess_api.h:10`
**문제**: `XPE_PREPROCESS_API_H_NEW` (unusual suffix)
**수정**: `XPE_PREPROCESS_API_H_` 로 변경

---

## 경미한 문제 (선택사항)

### 9. [낮음] Magic 상수 설명 부족

`0xA7057AC0u` — 왜 이 값인지 주석 추가 권장

---

## @MX 태그 현황

- 7개 @MX:ANCHOR 태그 (공개 API 진입점)
- 2개 @MX:NOTE 태그 (컨텍스트)
- 0개 @MX:TODO 태그 (모두 구현 완료)
- 모든 ANCHOR 태그에 @MX:REASON 포함 ✓

---

## 테스트 커버리지 요약

**테스트 파일**: 9개, 943줄 총계
- Happy path: ~85%
- 오류 경로: ~40% (malloc/파일 I/O 실패 미테스트)
- 경계 조건: ~60% (stride/오버플로우 미테스트)
- 통합: 좋음 (7단계 파이프라인 end-to-end 테스트)

**추정 문장 커버리지**: 70-75%

---

## 커밋 전 필수 조치

### 차단 (커밋 전 필수)
1. gain_correct 오버플로우 검사 추가
2. calibration FILE* 누수 수정
3. stride 검증 추가 또는 문서화
4. gain_correct 소유권 이전 API 문서 업데이트

### 권장 (커밋 전 강력 권장)
5. malloc 실패 테스트 추가
6. CRC 불일치 테스트 추가
7. Ghost 핸들 use-after-free 문서화
8. stride 검증 테스트 추가

---

## 규정 준수 상태

- **IEC 62304 Class B**: 부분 — 리소스 안전 문제 해결 필요
- **TRUST 5 Readable**: 통과 (82/100)
- **TRUST 5 Trackable**: 통과 (80/100)
- **TRUST 5 Tested**: 경고 (65/100)
- **TRUST 5 Unified**: 경고 (78/100)
- **TRUST 5 Secured**: **심각** (58/100)

**최종 결정: CRITICAL — 병합 금지. 보안 및 안정성 수정 필수.**

---

## 다음 단계

1. 위 항목 1-4의 수정 구현
2. 항목 5-8의 테스트 추가
3. 업데이트된 코드로 품질 게이트 재실행
4. 수정된 SPEC-XPE-P1A 재검증 제출
