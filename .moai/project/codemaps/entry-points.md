# XPE 진입점 및 API 인터페이스

**문서 ID**: XPE-CODEMAP-004  
**버전**: 1.0.0  
**날짜**: 2026-04-17  
**상태**: 작성 중  
**분류**: 내부 / 진입점 문서

---

## 1. 진입점 개요

XPE 아키텍처는 82개의 C ABI 함수를 통해 외부와 상호작용합니다. 이 진입점들은 DLL 로딩부터 이미지 처리, 오류 처리까지의 전체 라이프사이클을 커버합니다.

### 1.1 진입점 구성

| DLL 이름 | 함수 개수 | 진입점 유형 | 목적 |
|----------|-----------|-------------|------|
| `xpe_common.dll` | 18개 | 시스템 진입점 | 라이브러리 관리, 메모리, 구성 |
| `xpe_preprocess.dll` | 18개 | 처리 진입점 | 전처리 알고리즘 |
| `xpe_enhance_basic.dll` | 6개 | 처리 진입점 | 기본 향상 알고리즘 |
| `xpe_enhance_advanced.dll` | 4개 | 처리 진입점 | 고급 향상 알고리즘 |
| `xpe_ai.dll` | 7개 | 처리 진입점 | AI 알고리즘 |
| `xpe_display.dll` | 11개 | 처리 진입점 | 디스플레이 LUT |
| `xpe_dicom.dll` | 10개 | I/O 진입점 | DICOM 파일 처리 |
| `gsvg.dll` | 8개 | 처리 진입점 | 그리드 처리 |
| **총계** | **82개** | - | 전체 API |

---

## 2. 응용 프로그램 진입점

### 2.1 라이브러리 라이프사이클 (xpe_common.dll)

| 진입점 | 함수 시그니처 | 설명 |
|--------|---------------|------|
| **초기화** | `XpeErrorCode xpe_init(const char* configJsonOrNull)` | XPE 서브시스템 초기화 |
| **종료** | `void xpe_shutdown(void)` | 모든 XPE 리소스 해제 |
| **버전 정보** | `const char* xpe_version(void)` | 버전 정보 반환 |
| **구성 관리** | `XpeErrorCode xpe_configure(const char* jsonConfig)` | 런타임 구성 업데이트 |

**사용 예시**:
```csharp
// C# P/Invoke 호출
[DllImport("xpe_common.dll", CallingConvention = CallingConvention.Cdecl)]
private static extern XpeErrorCode xpe_init([MarshalAs(UnmanagedType.LPStr)] string configJson);

// 초기화
XpeErrorCode result = xpe_init(null); // 기본 설정 사용
if (result == XpeErrorCode.OK) {
    Console.WriteLine("XPE 초기화 성공");
}
```

### 2.2 메모리 관리 진입점

| 진입점 | 함수 시그니처 | 설명 |
|--------|---------------|------|
| **할당** | `XpeErrorCode xpe_alloc_image(uint32_t width, uint32_t height, XpePixelFormat format, XpeImageBuffer* out)` | 이미지 버퍼 할당 |
| **해제** | `XpeErrorCode xpe_free_image(XpeImageBuffer* buf)` | 이미지 버퍼 해제 |
| **복사** | `XpeErrorCode xpe_copy_image(const XpeImageBuffer* src, XpeImageBuffer* dst)` | 이미지 복사 |

**사용 예시**:
```csharp
// 이미지 버퍼 구조체 정의
[StructLayout(LayoutKind.Sequential, Pack = 8)]
public struct XpeImageBuffer {
    public uint width;
    public uint height;
    public uint bitsAllocated;
    public uint bitsStored;
    public XpePixelFormat format;
    public IntPtr data; // void*
    public ulong dataSize;
}

[DllImport("xpe_common.dll")]
private static extern XpeErrorCode xpe_alloc_image(uint width, uint height, XpePixelFormat format, out XpeImageBuffer img);

// 버퍼 할당
XpeImageBuffer buffer = new XpeImageBuffer();
XpeErrorCode result = xpe_alloc_image(1920, 1080, XpePixelFormat.XPE_PIXEL_UINT16, out buffer);
```

---

## 3. 처리 진입점

### 3.1 전처리 단계 진입점 (xpe_preprocess.dll)

| 진입점 | 함수 시그니처 | 처리 단계 | 주요 파라미터 |
|--------|---------------|-----------|---------------|
| **오프셋 보정** | `XpeErrorCode xpe_offset_correct(XpeImageBuffer* img, const XpeImageBuffer* offsetMap)` | PRE-02 | 오프셋 맵 |
| **게인 보정** | `XpeErrorCode xpe_gain_correct(XpeImageBuffer* img, const XpeImageBuffer* gainMap)` | PRE-03 | 게인 맵 |
| **결함 보정** | `XpeErrorCode xpe_defect_correct(XpeImageBuffer* img, const XpeImageBuffer* defectMap, const char* configJsonOrNull)` | PRE-06 | 결함 맵 |
| **고스트 보정** | `XpeErrorCode xpe_ghost_correct(void* handle, XpeImageBuffer* img, const XpeImageMetadata* meta)` | PRE-09 | 고스트 핸들 |
| **온도 보상** | `XpeErrorCode xpe_temp_compensate(XpeImageBuffer* img, float detectorTempC, const char* configJsonOrNull)` | PRE-07 | 온도 값 |
| **비선형 보정** | `XpeErrorCode xpe_nonlinearity_correct(XpeImageBuffer* img, const char* configJsonOrNull)` | PRE-08 | 설정 JSON |

**처리 순서 강제**:
```csharp
// 정해진 순서대로 호출해야 함
public void PreprocessPipeline(XpeImageBuffer rawImage, float temperature) {
    // 1. 오프셋 보정
    xpe_offset_correct(rawImage, offsetMap);
    
    // 2. 게인 보정 (uint16 -> float32 변환)
    xpe_gain_correct(rawImage, gainMap);
    
    // 3. 온도 보상
    xpe_temp_compensate(rawImage, temperature);
    
    // 4. 결함 보정
    xpe_defect_correct(rawImage, defectMap, config);
    
    // 5. 고스트 보정
    xpe_ghost_correct(ghostHandle, rawImage, metadata);
}
```

### 3.2 향상 단계 진입점 (xpe_enhance_basic.dll)

| 진입점 | 함수 시그니처 | 처리 단계 | 주요 파라미터 |
|--------|---------------|-----------|---------------|
| **로그 변환** | `XpeErrorCode xpe_log_transform(XpeImageBuffer* img, const char* configJsonOrNull)` | POST-01 | 로그 파라미터 |
| **노이즈 감소** | `XpeErrorCode xpe_noise_reduce(XpeImageBuffer* img, const char* configJsonOrNull)` | POST-02 | 노이즈 설정 |
| **대비 향상** | `XpeErrorCode xpe_contrast_enhance(XpeImageBuffer* img, const char* configJsonOrNull)` | POST-03 | CLAHE 설정 |
| **엣지 향상** | `XpeErrorCode xpe_edge_enhance(XpeImageBuffer* img, const char* configJsonOrNull)` | POST-04 | 엣지 설정 |

**사용 예시**:
```csharp
// 로그 변환 + 노이즈 감소
public void EnhanceBasic(XpeImageBuffer floatImage) {
    // 1. 로그 변환 (float32에서 처리)
    string logConfig = "{\"base\": 10.0, \"offset\": 1.0}";
    xpe_log_transform(floatImage, logConfig);
    
    // 2. 노이즈 감소
    string noiseConfig = "{\"method\": \"bilateral\", \"sigma\": 15.0, \"diameter\": 7}";
    xpe_noise_reduce(floatImage, noiseConfig);
    
    // 3. 대비 향상
    string contrastConfig = "{\"clipLimit\": 2.0, \"tileSize\": 8}";
    xpe_contrast_enhance(floatImage, contrastConfig);
    
    // 4. 엣지 향상
    string edgeConfig = "{\"strength\": 1.0, \"radius\": 2}";
    xpe_edge_enhance(floatImage, edgeConfig);
}
```

### 3.3 고급 처리 진입점 (xpe_enhance_advanced.dll)

| 진입점 | 함수 시그니처 | 처리 단계 | 주요 파라미터 |
|--------|---------------|-----------|---------------|
| **다중 스케일 처리** | `XpeErrorCode xpe_multiscale_process(XpeImageBuffer* img, const XpeImageMetadata* meta, const char* configJsonOrNull)` | POST-05/08 | 메타데이터 |
| **분수 처리** | `XpeErrorCode xpe_fractional_process(XpeImageBuffer* img, float order, const char* configJsonOrNull)` | POST-06 | 차수 |
| **콜리메이션 검출** | `XpeErrorCode xpe_detect_collimation(const XpeImageBuffer* img, int32_t* x0, int32_t* y0, int32_t* x1, int32_t* y1, const char* configJsonOrNull)` | POST-09 | 경계 좌표 |
| **노출 인덱스 계산** | `XpeErrorCode xpe_calc_exposure_index(const XpeImageBuffer* img, const XpeImageMetadata* meta, float* eiOut, float* diOut)` | POST-10 | EI/DI 값 |

**ROI 기반 처리**:
```csharp
// 콜리메이션 ROI 기반 처리
public void ProcessWithROI(XpeImageBuffer image, XpeImageMetadata metadata) {
    // 1. 콜리메이션 검출
    int x0, y0, x1, y1;
    xpe_detect_collimation(image, out x0, out y0, out x1, out y1);
    
    // 2. ROI 정보 메타데이터에 저장
    metadata.collimationROI = new Rectangle(x0, y0, x1 - x0, y1 - y0);
    
    // 3. ROI 제한 노출 인덱스 계산
    float ei, di;
    xpe_calc_exposure_index(image, metadata, out ei, out di);
    
    // EI/DI 값에 따른 후속 처리...
}
```

### 3.4 AI 처리 진입점 (xpe_ai.dll)

| 진입점 | 함수 시그니처 | 처리 단계 | 주요 파라미터 |
|--------|---------------|-----------|---------------|
| **AI 초기화** | `XpeErrorCode xpe_ai_init(const char* modelDirPath, const char* configJsonOrNull)` | - | 모델 디렉토리 |
| **AI 종료** | `void xpe_ai_shutdown(void)` | - | 없음 |
| **신체 부위 인식** | `XpeErrorCode xpe_bodypart_recognize(const XpeImageBuffer* img, char* bodyPartOut, size_t bufLen, float* confidenceOut)` | POST-01 | 신체 부위, 신뢰도 |
| **이미지 스티칭** | `XpeErrorCode xpe_stitch_images(const XpeImageBuffer* parts, uint32_t partCount, XpeImageBuffer* stitchedOut, const char* configJsonOrNull)` | POST-02 | 부분 이미지들 |
| **뼈 억제** | `XpeErrorCode xpe_bone_suppress(const XpeImageBuffer* img, XpeImageBuffer* softTissueOut, const char* configJsonOrNull)` | POST-03 | 연부조직 이미지 |
| **DL 노이즈 감소** | `XpeErrorCode xpe_dl_denoise(XpeImageBuffer* img, const XpeImageMetadata* meta, const char* configJsonOrNull)` | POST-04 | 메타데이터 |

**AI 워커 관리**:
```csharp
// AI 초기화 및 처리
public class AIProcessor {
    private bool aiInitialized = false;
    
    public void InitializeAI() {
        if (!aiInitialized) {
            XpeErrorCode result = xpe_ai_init("C:\\models\\bodypart", null);
            if (result == XpeErrorCode.OK) {
                aiInitialized = true;
            }
        }
    }
    
    public string RecognizeBodyPart(XpeImageBuffer image) {
        if (!aiInitialized) throw new InvalidOperationException("AI not initialized");
        
        StringBuilder bodyPart = new StringBuilder(64);
        float confidence;
        xpe_bodypart_recognize(image, bodyPart, 64, out confidence);
        
        return bodyPart.ToString();
    }
}
```

---

## 4. 디스플레이 진입점 (xpe_display.dll)

| 진입점 | 함수 시그니처 | 처리 단계 | 주요 파라미터 |
|--------|---------------|-----------|---------------|
| **모달리티 LUT 적용** | `XpeErrorCode xpe_modality_lut_apply(XpeImageBuffer* img, float rescaleSlope, float rescaleIntercept)` | POST-11 | 기울기, 절편 |
| **VOI LUT 적용** | `XpeErrorCode xpe_voi_lut_apply(XpeImageBuffer* img, float windowCenter, float windowWidth, int32_t function)` | POST-12 | 중심, 너비, 함수 |
| **프레젠테이션 LUT** | `XpeErrorCode xpe_presentation_lut_apply(XpeImageBuffer* img, const char* presetNameOrNull, const char* configJsonOrNull)` | POST-13 | 프리셋 이름 |
| **LUT 프리셋 관리** | `XpeErrorCode xpe_lut_add_custom_preset(const char* name, const char* description, const char* lutDefinitionJson)` | - | 프리셋 정보 |

**실시간 디스플레이 처리**:
```csharp
public class DisplayProcessor {
    public void ApplyWindowLevel(XpeImageBuffer image, float windowCenter, float windowWidth) {
        // VOI LUT 적용 (실시간)
        xpe_voi_lut_apply(image, windowCenter, windowWidth, 0); // LINEAR
        
        // 프레젠테이션 LUT 적용
        xpe_presentation_lut_apply(image, "GSDF", null);
    }
    
    public void CreateCustomPreset(string name, string description, float[] lutData) {
        string lutJson = $"{{\"preset\": \"{name}\", \"lut\": [{string.Join(",", lutData)}]}}";
        xpe_lut_add_custom_preset(name, description, lutJson);
    }
}
```

---

## 5. DICOM 진입점 (xpe_dicom.dll)

| 진입점 | 함수 시그니처 | 처리 단계 | 주요 파라미터 |
|--------|---------------|-----------|---------------|
| **DICOM 읽기** | `XpeErrorCode xpe_dicom_read(const char* filePath, XpeImageBuffer* imgOut, XpeImageMetadata* metaOut)` | SUP-01 | 파일 경로 |
| **DICOM 쓰기** | `XpeErrorCode xpe_dicom_write(const char* filePath, const XpeImageBuffer* img, const XpeImageMetadata* meta, const char* configJsonOrNull)` | SUP-02 | 파일 경로 |
| **C-STORE 전송** | `XpeErrorCode xpe_dicom_cstore(const char* filePath, const char* remoteAeTitle, const char* remoteHost, uint16_t remotePort, const char* localAeTitle)` | SUP-03 | 네트워크 정보 |
| **GSPS 생성** | `XpeErrorCode xpe_gsps_create(const char* referencedFilePath, const char* annotationJson, char* gspsFilePathOut, size_t gspsPathBufLen)` | SUP-04 | 주석 정보 |

**DICOM 파일 처리**:
```csharp
public class DicomProcessor {
    public XpeImageMetadata LoadDicom(string filePath, out XpeImageBuffer image) {
        image = new XpeImageBuffer();
        XpeImageMetadata metadata = new XpeImageMetadata();
        
        XpeErrorCode result = xpe_dicom_read(filePath, out image, metadata);
        if (result != XpeErrorCode.OK) {
            throw new Exception("DICOM read failed");
        }
        
        return metadata;
    }
    
    public void SaveDicom(string filePath, XpeImageBuffer image, XpeImageMetadata metadata) {
        string config = "{\"transferSyntax\": \"1.2.840.10008.1.2.4.90\"}"; // JPEG2000
        XpeErrorCode result = xpe_dicom_write(filePath, image, metadata, config);
        
        if (result != XpeErrorCode.OK) {
            throw new Exception("DICOM write failed");
        }
    }
    
    public void SendToPACS(string filePath, string aeTitle, string host, int port) {
        xpe_dicom_cstore(filePath, aeTitle, host, (ushort)port, "LOCAL_AE");
    }
}
```

---

## 6. 그리드 처리 진입점 (gsvg.dll)

| 진입점 | 함수 시그니처 | 처리 단계 | 주요 파라미터 |
|--------|---------------|-----------|---------------|
| **그리드 처리** | `GsvgErrorCode gsvg_process(uint16_t* pixels, uint32_t width, uint32_t height, const GsvgConfig* config)` | GSVG-01 | 픽셀 데이터 |
| **그리드 검출** | `GsvgErrorCode gsvg_detect_grid(const uint16_t* pixels, uint32_t width, uint32_t height, float pixelPitch_mm, int32_t* freqOut, float* angleOut)` | GSVG-02 | 주파수, 각도 |
| **그리드 억제** | `GsvgErrorCode gsvg_suppress_grid(uint16_t* pixels, uint32_t width, uint32_t height, int32_t freq_lp_per_mm, float angle_deg, float suppressionStrength)` | GSVG-03 | 억제 강도 |
| **가상 그리드** | `GsvgErrorCode gsvg_virtual_grid(uint16_t* pixels, uint32_t width, uint32_t height, const GsvgImageMetadata* meta, const char* configJsonOrNull)` | GSVG-04 | 메타데이터 |

**독립적인 그리드 처리**:
```csharp
public class GridProcessor {
    public void ProcessGridImage(IntPtr pixelData, int width, int height, float pixelPitch) {
        GsvgConfig config = new GsvgConfig {
            gridFrequency_lp_per_mm = 0, // 자동 검출
            gridAngle_deg = 0,
            algorithmMode = 0, // Auto
            suppressionStrength = 0.8f,
            enableVirtualGrid = 1
        };
        
        // 그리드 처리
        GsvgErrorCode result = gsvg_process(pixelData.ToPointer(), (uint)width, (uint)height, config);
        
        if (result == GsvgErrorCode.GSVG_ERR_GRID_NOT_DETECTED) {
            // 가상 그리드 생성
            gsvg_virtual_grid(pixelData.ToPointer(), (uint)width, (uint)height, metadata, null);
        }
    }
}
```

---

## 7. 이벤트 처리 진입점

### 7.1 알릿 시스템 (xpe_common.dll)

| 진입점 | 함수 시그니처 | 목적 |
|--------|---------------|------|
| **알릿 개수 확인** | `int32_t xpe_get_pending_alert_count(void)` | 대기 중인 알릿 수 |
| **알릿 조회** | `XpeErrorCode xpe_get_pending_alert(int32_t index, char* msg, size_t msgLen, int32_t* severity)` | 특정 알릿 조회 |
| **알릿 초기화** | `void xpe_clear_alerts(void)` | 모든 알릿 삭제 |

**알릿 처리**:
```csharp
public void ProcessAlerts() {
    int alertCount = xpe_get_pending_alert_count();
    
    for (int i = 0; i < alertCount; i++) {
        StringBuilder message = new StringBuilder(256);
        int severity;
        
        XpeErrorCode result = xpe_get_pending_alert(i, message, 256, out severity);
        
        if (result == XpeErrorCode.OK) {
            AlertSeverity level = (AlertSeverity)severity;
            string alertMessage = message.ToString();
            
            // UI에 알릿 표시
            ShowAlertToUI(level, alertMessage);
        }
    }
    
    // 처리된 알릿 삭제
    xpe_clear_alerts();
}
```

### 7.2 AED 시스템 (xpe_common.dll)

| 진입점 | 함수 시그니처 | 목적 |
|--------|---------------|------|
| **AED 구성** | `XpeErrorCode xpe_aed_configure(const char* configJsonOrNull)` | 자동 노출 감지 구성 |
| **AED 이벤트 폴링** | `XpeErrorCode xpe_aed_poll_event(int32_t* eventTypeOut, uint64_t* timestampOut, float* signalLevelOut)` | 노출 이벤트 확인 |
| **AED 상태 확인** | `XpeErrorCode xpe_aed_get_status(int32_t* stateOut)` | AED 상태 확인 |

**AED 이벤트 처리**:
```csharp
public class AEDProcessor {
    public void ConfigureAED() {
        string config = @"{
            ""enabled"": true,
            ""doseThreshold"": 100.0,
            ""cooldownPeriodMs"": 5000,
            ""callbackMode"": ""poll""
        }";
        
        xpe_aed_configure(config);
    }
    
    public bool CheckExposureEvent() {
        int eventType;
        uint64_t timestamp;
        float signalLevel;
        
        XpeErrorCode result = xpe_aed_poll_event(out eventType, out timestamp, out signalLevel);
        
        if (result == XpeErrorCode.OK) {
            // 노출 이벤트 발생
            DateTime eventTime = DateTimeOffset.FromUnixTimeMilliseconds(timestamp).DateTime;
            Console.WriteLine($"Exposure detected at {eventTime}, level: {signalLevel}");
            return true;
        }
        
        return false;
    }
}
```

---

## 8. P/Invoke 인터페이스 정의

### 8.1 C# P/Invoke 시그니처

```csharp
// 기본 타입 정의
public enum XpePixelFormat {
    XPE_PIXEL_UINT16 = 0,
    XPE_PIXEL_FLOAT32 = 1
}

public enum XpeAlertSeverity {
    INFO = 0,
    WARN = 1,
    ERROR = 2
}

public enum XpeErrorCode {
    OK = 0,
    ERR_INVALID_INPUT = -1,
    ERR_OUT_OF_MEMORY = -2,
    ERR_PROCESSING_FAILED = -3,
    ERR_CONFIG_INVALID = -4,
    ERR_CALIBRATION_EXPIRED = -5,
    ERR_NOT_INITIALIZED = -6,
    ERR_UNSUPPORTED_FORMAT = -7,
    ERR_BUFFER_TOO_SMALL = -8,
    ERR_IO_FAILED = -9,
    ERR_NETWORK_FAILED = -10
}

// 메모리 관리
[DllImport("xpe_common.dll", CallingConvention = CallingConvention.Cdecl)]
public static extern XpeErrorCode xpe_alloc_image(uint width, uint height, XpePixelFormat format, out XpeImageBuffer img);

[DllImport("xpe_common.dll", CallingConvention = CallingConvention.Cdecl)]
public static extern XpeErrorCode xpe_free_image(ref XpeImageBuffer img);

// 이미지 처리
[DllImport("xpe_preprocess.dll", CallingConvention = CallingConvention.Cdecl)]
public static extern XpeErrorCode xpe_offset_correct(ref XpeImageBuffer img, XpeImageBuffer offsetMap);

[DllImport("xpe_enhance_basic.dll", CallingConvention = CallingConvention.Cdecl)]
public static extern XpeErrorCode xpe_log_transform(ref XpeImageBuffer img, string configJson);

// AI 처리
[DllImport("xpe_ai.dll", CallingConvention = CallingConvention.Cdecl)]
public static extern XpeErrorCode xpe_ai_init(string modelDirPath, string configJson);

[DllImport("xpe_ai.dll", CallingConvention = CallingConvention.Cdecl)]
public static extern XpeErrorCode xpe_bodypart_recognize(XpeImageBuffer img, StringBuilder bodyPart, int bufLen, out float confidence);
```

### 8.2 구조체 패킹

```csharp
// 이미지 버퍼 (Pack=8로 8바이트 정렬)
[StructLayout(LayoutKind.Sequential, Pack = 8)]
public struct XpeImageBuffer {
    public uint width;              // 4바이트
    public uint height;             // 4바이트
    public uint bitsAllocated;      // 4바이트
    public uint bitsStored;         // 4바이트
    public XpePixelFormat format;   // 4바이트
    public IntPtr data;             // 8바이트 (void*)
    public ulong dataSize;          // 8바이트
    // 총 40바이트
}

// 이미지 메타데이터
[StructLayout(LayoutKind.Sequential, Pack = 8)]
public struct XpeImageMetadata {
    [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 64)]
    public string bodyPart;         // 64바이트
    public float kVp;               // 4바이트
    public float mAs;               // 4바이트
    public float SID_mm;            // 4바이트
    public float pixelPitch_mm;     // 4바이트
    public ulong acquisitionTime;   // 8바이트
    public uint flags;             // 4바이트
    // 총 96바이트
}
```

---

## 9. 오류 처리 진입점

### 9.1 오류 코드 진입점

| 진입점 | 함수 시그니처 | 목적 |
|--------|---------------|------|
| **오류 문자열** | `const char* xpe_error_string(XpeErrorCode code)` | 오류 설명 문자열 |
| **오류 범위** | 각 DLL별 오류 코드 | 구체적 오류 정보 |

**오류 처리 패턴**:
```csharp
public XpeImageBuffer ProcessImage(XpeImageBuffer input) {
    try {
        // 처리 시작
        xpe_log_transform(ref input, null);
        
        // 각 단계별 오류 확인
        XpeErrorCode result = xpe_noise_reduce(ref input, noiseConfig);
        if (result != XpeErrorCode.OK) {
            string error = xpe_error_string(result);
            throw new ProcessingException($"Noise reduction failed: {error}");
        }
        
        return input;
    }
    catch (Exception ex) {
        // 오류 로깅
        LogError($"Processing failed: {ex.Message}");
        
        // 알릿 시스템에 추가
        AddAlert(XpeAlertSeverity.ERROR, ex.Message);
        
        throw;
    }
}
```

---

## 10. 최종 진입점 통합 예제

```csharp
public class XPEProcessor {
    private bool initialized = false;
    private IntPtr ghostHandle = IntPtr.Zero;
    
    public void Initialize() {
        // 1. 시스템 초기화
        XpeErrorCode result = xpe_init(null);
        if (result != XpeErrorCode.OK) {
            throw new InitializationException("XPE initialization failed");
        }
        
        // 2. 고스트 보정기 초기화
        xpe_ghost_create(1920, 1080, null, out ghostHandle);
        
        initialized = true;
    }
    
    public DicomImage ProcessRawImage(IntPtr pixelData, int width, int height, float temperature) {
        if (!initialized) throw new InvalidOperationException("XPE not initialized");
        
        // 1. 이미지 버퍼 할당
        XpeImageBuffer buffer;
        xpe_alloc_image((uint)width, (uint)height, XpePixelFormat.XPE_PIXEL_UINT16, out buffer);
        
        // 2. 원본 데이터 복사
        CopyPixelData(buffer.data, pixelData, width, height);
        
        // 3. 전처리 파이프라인
        PreprocessPipeline(buffer, temperature);
        
        // 4. 향상 처리
        EnhancePipeline(buffer);
        
        // 5. 디스플레이 처리
        DisplayPipeline(buffer);
        
        // 6. DICOM 출력
        return CreateDicomImage(buffer);
    }
    
    public void Shutdown() {
        if (initialized) {
            // 1. 고스트 보정기 해제
            if (ghostHandle != IntPtr.Zero) {
                xpe_ghost_destroy(ghostHandle);
                ghostHandle = IntPtr.Zero;
            }
            
            // 2. AI 종료
            if (aiInitialized) {
                xpe_ai_shutdown();
                aiInitialized = false;
            }
            
            // 3. 시스템 종료
            xpe_shutdown();
            initialized = false;
        }
    }
}
```

---

## 11. 참고 문서

- `.moai/project/api-spec.md` - 완전한 C API 명세
- `.moai/project/pipeline-spec.md` - 처리 파이프라인 명세
- `docs/` - 각 DLL별 상세 문서

---

*최종 업데이트: 2026-04-17*