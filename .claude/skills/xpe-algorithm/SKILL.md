---
name: xpe-algorithm
description: "X-ray FPD 이미지 처리 알고리즘 구현 패턴. Ghost correction, Gain/Offset, Defect map, CLAHE, Windowing, SIMD AVX2 최적화, LUT, Deterministic 재현성, 벤치마크 BP-01~10. '알고리즘 구현', 'Ghost correction', 'SIMD', 'gain offset', 'CLAHE', '벤치마크' 요청 시 반드시 트리거."
---

# XPE Algorithm

X-ray 이미지 처리 알고리즘 구현. Scalar reference → SIMD 최적화 순서를 반드시 따른다.

## 알고리즘 우선순위

```
Phase 1a: Ghost correction, Gain/Offset calibration, Defect pixel correction
Phase 1b: CLAHE, Unsharp masking, Windowing (VOI LUT)
Phase 2: Advanced noise reduction, Stitching
Phase 3: AI inference proxy (모델 로드만, 자체 구현 없음)
```

## Scalar Reference 패턴 (항상 먼저)

```cpp
// Ghost correction scalar reference
// lag_frames개의 이전 프레임을 가중합산하여 잔상 제거
void ghost_corr_scalar(const uint16_t* curr,        // 현재 프레임
                       uint16_t* out,               // 출력
                       const uint16_t** prev_frames, // 이전 프레임 배열
                       const float* weights,         // 가중치
                       int n_lag,                    // lag 수
                       uint32_t npix) {
    for (uint32_t i = 0; i < npix; ++i) {
        float acc = curr[i];
        for (int k = 0; k < n_lag; ++k)
            acc -= weights[k] * prev_frames[k][i];
        out[i] = static_cast<uint16_t>(std::clamp(acc, 0.0f, 65535.0f));
    }
}
```

## Gain/Offset Calibration

```cpp
// flat-field correction: corrected = (raw - offset) / gain
// gain, offset: float32 calibration map (동일 크기)
void gain_offset_scalar(const uint16_t* raw,
                        uint16_t* out,
                        const float* gain,
                        const float* offset,
                        uint32_t npix) {
    for (uint32_t i = 0; i < npix; ++i) {
        float v = (raw[i] - offset[i]) / (gain[i] + 1e-6f);  // 0 나눗셈 방지
        out[i] = static_cast<uint16_t>(std::clamp(v, 0.0f, 65535.0f));
    }
}
```

## CLAHE 패턴

```cpp
// Contrast Limited Adaptive Histogram Equalization
// tile_size: 64x64 기본값
// clip_limit: 4.0 기본값
struct ClaheParams {
    int tile_size = 64;
    float clip_limit = 4.0f;
};

void clahe_apply(const uint16_t* src, uint16_t* dst,
                 uint32_t width, uint32_t height,
                 const ClaheParams& params);
```

## LUT 가속화 패턴

```cpp
// Windowing (VOI LUT) — 65536-entry LUT for uint16
void build_windowing_lut(float wc, float ww, uint8_t lut[65536]) {
    float lo = wc - ww * 0.5f;
    float hi = wc + ww * 0.5f;
    for (int i = 0; i < 65536; ++i) {
        float v = (i - lo) / (ww + 1e-6f);
        lut[i] = static_cast<uint8_t>(std::clamp(v * 255.0f, 0.0f, 255.0f));
    }
}

// 적용: O(N) with precomputed LUT
void apply_windowing_lut(const uint16_t* src, uint8_t* dst,
                         const uint8_t lut[65536], uint32_t npix) {
    for (uint32_t i = 0; i < npix; ++i)
        dst[i] = lut[src[i]];
}
```

## SIMD AVX2 패턴 (scalar 검증 후)

```cpp
#ifdef XPE_SIMD_AVX2
#include <immintrin.h>

// AVX2 gain/offset: 16 uint16 픽셀 동시 처리
void gain_offset_avx2(const uint16_t* raw, uint16_t* out,
                      const float* gain, const float* offset,
                      uint32_t npix) {
    uint32_t i = 0;
    for (; i + 16 <= npix; i += 16) {
        // 16x uint16 → 2x 8x float32
        __m256i raw_v = _mm256_loadu_si256((__m256i*)(raw + i));
        // ... AVX2 처리
    }
    // 나머지는 scalar로 처리
    gain_offset_scalar(raw + i, out + i, gain + i, offset + i, npix - i);
}
#endif
```

## Deterministic 보장 규칙

- float 연산 순서 고정 (`-ffp-contract=off` 또는 명시적 괄호)
- 플랫폼 독립: `std::round()` 사용, `(int)` 캐스팅 금지
- PRNG 사용 시 고정 seed 제공
- `NaN`/`Inf` 입력 시 `XPE_ERR_INVALID_INPUT` 반환

## 벤치마크 패턴 (BP 시리즈)

```cpp
// BP-01: Gain/Offset 처리량 (4096x4096, float32 calibration)
TEST(Benchmark, BP01_GainOffset_4k) {
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 100; ++i)
        gain_offset_scalar(raw.data(), out.data(), gain.data(), offset.data(), 4096*4096);
    auto elapsed = std::chrono::high_resolution_clock::now() - start;
    // 기준: < 50ms per frame (AVX2) at 4096x4096
    EXPECT_LT(elapsed.count() / 100, 50'000'000);  // ns
}
```

## references/ 참조

- `docs/project/xpe-algorithm-spec-deepsync.md` — 상세 알고리즘 스펙
- `docs/project/Algorithm-Benchmark-Pack-Spec.md` — BP-01~10 기준값
- `docs/project/Algorithm-Evaluation-Protocol.md` — 평가 프로토콜
