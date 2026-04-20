# XPE Testing Strategy

## Test Organization

### Test Directory Structure

```
tests/
├── CMakeLists.txt                    # Master test configuration
├── common/                           # Common module tests
│   ├── test_xpe_common.cpp          # Common API tests
│   └── CMakeLists.txt
├── common_smoke/                     # Smoke tests
│   ├── test_common_smoke.cpp        # Quick sanity checks
│   └── CMakeLists.txt
├── common_unit/                      # Unit tests
│   ├── test_xpe_common.cpp          # Detailed unit tests
│   └── CMakeLists.txt
└── enhance_advanced_tests/           # Advanced module tests
    ├── test_api_header.cpp          # C ABI verification
    ├── test_lifecycle.cpp           # Init/shutdown cycles
    ├── test_mfp_scalar.cpp          # MFP scalar implementation
    ├── test_collimation_detect.cpp  # Collimation detection
    ├── test_edge_enhancement.cpp    # Edge enhancement
    ├── test_exposure_index.cpp      # Exposure index
    ├── test_integration.cpp         # End-to-end integration
    └── CMakeLists.txt
```

## Test Categories

### 1. API Header Tests

**Purpose**: Verify C ABI compatibility

**File**: `test_api_header.cpp`

**Coverage**:
- Exported functions exist and have correct signatures
- Struct packing is correct (pack=8)
- No C++ name mangling
- All exported symbols are accounted for

**Example**:
```cpp
TEST(EnhanceAdvancedApi, VersionReturnsNonNull) {
    const char* ver = xpe_enhance_advanced_version();
    EXPECT_NE(ver, nullptr);
    EXPECT_STRNE(ver, "");
}
```

### 2. Lifecycle Tests

**Purpose**: Verify init/shutdown behavior

**File**: `test_lifecycle.cpp`

**Coverage**:
- Single init/shutdown cycle
- Multiple init/shutdown cycles
- Shutdown without init (no-op)
- Function calls before init (error)
- Function calls after shutdown (error)

**Example**:
```cpp
TEST(EnhanceAdvancedLifecycle, InitThenShutdown) {
    EXPECT_EQ(xpe_enhance_advanced_init(nullptr), XPE_OK);
    EXPECT_NO_THROW(xpe_enhance_advanced_shutdown());
}
```

### 3. Pixel Accuracy Tests

**Purpose**: Verify scalar vs SIMD equivalence

**File**: `test_mfp_scalar.cpp`

**Coverage**:
- Scalar implementation produces correct output
- SIMD implementation produces same output as scalar
- Deterministic behavior (same input → same output)

**Example**:
```cpp
TEST(MFPScalar, SameAsSIMD) {
    // Create test image
    XpeImage* input = create_test_image(512, 512);

    // Process with scalar
    XpeImage* output_scalar = process_scalar(input);

    // Process with SIMD
    XpeImage* output_simd = process_simd(input);

    // Verify pixel-perfect match
    EXPECT_EQ(compare_images(output_scalar, output_simd), 0);
}
```

### 4. Algorithm Tests

**Purpose**: Verify algorithm correctness

**Files**:
- `test_collimation_detect.cpp`: Collimation detection
- `test_edge_enhancement.cpp`: Edge enhancement
- `test_exposure_index.cpp`: Exposure index calculation

**Coverage**:
- Normal cases
- Edge cases (empty image, single pixel)
- Invalid inputs (NULL pointers)
- Boundary conditions

### 5. Integration Tests

**Purpose**: End-to-end workflow validation

**File**: `test_integration.cpp`

**Coverage**:
- Full pipeline execution
- Multi-module interaction
- Real-world use cases
- Performance benchmarks

## Test Execution

### Running All Tests

```bash
# Configure
cmake --preset default

# Build
cmake --build build/vs2022 --config Release

# Run tests
ctest --preset default -C Release
```

### Running Specific Tests

```bash
# Run only advanced module tests
ctest -R enhance_advanced -C Release

# Run specific test case
./tests/enhance_advanced_tests.exe --gtest_filter="MFPScalar.SameAsSIMD"
```

### Coverage Analysis

**Linux/Mac (GCC/Clang)**:
```bash
# Build with coverage
cmake -DBUILD_COVERAGE=ON ..
cmake --build .
ctest
lcov --capture --directory . --output-file coverage.info
genhtml coverage.info --output-directory coverage_html
```

**Windows (MSVC)**:
```bash
# Use OpenCppCoverage externally
OpenCppCoverage --sources modules/ -- ctest --preset default
```

## Test Data Management

### Test Fixtures

```cpp
class EnhanceAdvancedTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create test images
        input_ = create_test_image(2048, 2048);
        output_ = nullptr;
    }

    void TearDown() override {
        if (input_) xpe_free_image(input_);
        if (output_) xpe_free_image(output_);
    }

    XpeImage* input_;
    XpeImage* output_;
};
```

### Test Image Generation

```cpp
XpeImage* create_test_image(uint32_t width, uint32_t height) {
    XpeImage* img = nullptr;
    xpe_alloc_image(width, height, XPE_PIX_MONO16, &img);

    // Fill with test pattern
    uint16_t* data = static_cast<uint16_t*>(img->data);
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            data[y * img->stride / 2 + x] = static_cast<uint16_t>((x + y) % 4096);
        }
    }

    return img;
}
```

## Validation Criteria

### Pixel Accuracy

**Tolerance**:
- Scalar vs SIMD: 0 ULP (exact match required)
- Floating point operations: 1e-6 relative error

**Verification**:
```cpp
ASSERT_NEAR(pixel_scalar, pixel_simd, 0.0f);
```

### Performance Benchmarks

**Metrics**:
- Processing time (milliseconds)
- Throughput (megapixels/second)
- Memory usage (MB)

**Benchmarks**:
```cpp
TEST(MFPBenchmark, Process2048x2048) {
    auto start = std::chrono::high_resolution_clock::now();

    xpe_enhance_advanced_process(input_, output_, nullptr);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_LT(duration.count(), 100); // Must complete in <100ms
}
```

### Memory Leak Detection

**Tools**:
- Windows: Visual Studio Diagnostic Tools
- Linux: Valgrind
- Mac: Leaks instrument

**Test Pattern**:
```cpp
TEST(EnhanceAdvancedMemory, NoLeaks) {
    for (int i = 0; i < 1000; ++i) {
        XpeImage* img = create_test_image(512, 512);
        XpeImage* out = nullptr;
        xpe_enhance_advanced_process(img, out, nullptr);
        xpe_free_image(img);
        xpe_free_image(out);
    }
    // Check memory usage after loop
}
```

## Continuous Integration

### GitHub Actions Workflow

```yaml
name: XPE Tests

on: [push, pull_request]

jobs:
  test:
    runs-on: [windows-latest, ubuntu-latest]

    steps:
    - uses: actions/checkout@v3

    - name: Configure CMake
      run: cmake --preset default

    - name: Build
      run: cmake --build build --config Release

    - name: Test
      run: ctest --preset default -C Release

    - name: Upload Results
      uses: actions/upload-artifact@v3
      with:
        name: test-results
        path: build/Testing/
```

## Test Coverage Goals

### Module Coverage Targets

| Module | Statement Coverage | Branch Coverage | Function Coverage |
|--------|-------------------|-----------------|-------------------|
| common | 85% | 80% | 90% |
| preprocess | 80% | 75% | 85% |
| enhance_basic | 80% | 75% | 85% |
| enhance_advanced | 75% | 70% | 80% |
| display | 75% | 70% | 80% |
| dicom | 75% | 70% | 80% |

### Critical Path Coverage

**Must-Have Coverage**:
- All exported API functions: 100%
- Memory allocation/deallocation: 100%
- Error handling paths: 90%+
- SIMD vs Scalar equivalence: 100%

## Regression Testing

### Golden File Testing

```cpp
TEST(EnhanceAdvancedRegression, MatchesGoldenOutput) {
    // Load input from file
    XpeImage* input = load_image("test_data/input_2048x2048.raw");

    // Process
    XpeImage* output = nullptr;
    xpe_enhance_advanced_process(input, output, "config.json");

    // Compare to golden output
    XpeImage* golden = load_image("test_data/golden_2048x2048.raw");
    EXPECT_EQ(compare_images(output, golden), 0);

    // Cleanup
    xpe_free_image(input);
    xpe_free_image(output);
    xpe_free_image(golden);
}
```

### Differential Testing

Compare output across:
- Different compiler versions
- Different optimization levels
- Different platforms (Windows/Linux)

---

**Last Updated**: 2026-04-19
**Specification Version**: 0.1.0
