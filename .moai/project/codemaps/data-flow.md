# XPE 데이터 흐름 아키텍처

**문서 ID**: XPE-CODEMAP-005  
**버전**: 1.0.0  
**날짜**: 2026-04-17  
**상태**: 작성 중  
**분류**: 내부 / 데이터 흐름 문서

---

## 1. 데이터 흐름 개요

XPE는 원시 raw 프레임을 최종 DICOM 영상으로 변환하는 17단계 처리 파이프라인을 구현합니다. 데이터 흐름은 엄격한 형식 변환과 상태 관리를 따릅니다.

### 1.1 데이터 흐름 특징

- **형식 변환**: `uint16` → `float32` → `uint16` 단방향 흐름
- **상태 비설계**: 모든 처리 함수는 무상태(stateless) 설계
- **호출자 할당**: 메모리는 호출자가 관리
- **데이터 무결성**: 체크포인트 기반 데이터 검증

### 1.2 데이터 형식 경계

```
Raw Frame [uint16] 
  ↓
Pre-processing [uint16 → float32] (Format Boundary)
  ↓
Enhancement [float32] 
  ↓
Display LUT [float32 → uint16] (Format Boundary)
  ↓
DICOM Output [uint16]
```

---

## 2. 데이터 흐름 경로

### 2.1 표준 처리 파이프라인

```
Raw Frame
  ↓ (0.5) Readout Validation (PRE-01) - 신규 추가
  ↓ (0.7) Temperature Compensation (PRE-07) - 신규 추가
  ↓ (1) Offset Correction (PRE-02) — uint16
  ↓ (1.5) Nonlinearity Correction (PRE-08) - 신규 추가
  ↓ (2) Gain Correction (PRE-03) — uint16 → float32 [FORMAT BOUNDARY]
  ↓ (2.5) Binning Correction (PRE-09) - 신규 추가
  ↓ (3) Defect Correction (PRE-06) — float32
  ↓ (4) Ghost Correction (PRE-09) — float32 [TIER 1/2/3 NLCSC]
  ↓ (5) Log Transform (POST-01) — float32
  ↓ (5a) Body Part Recognition (POST-02 AI) — float32 → metadata
  ↓ (5b) Collimation Detection (POST-09) — float32 → ROI metadata
  ↓ (6) Noise Reduction (POST-02) — float32
  ↓ (7) Contrast Enhancement (POST-03) — float32
  ↓ (8) Edge Enhancement (POST-04) — float32
  ↓ (9) GSVG/Grid Suppression (GSVG-01) — float32
  ↓ (10) Multiscale Processing (POST-05/08) — float32
  ↓ (11) Fractional Processing (POST-06) — float32
  ↓ (12) Image Stitching (POST-02 AI) — float32
  ↓ (13) Bone Suppression (POST-03 AI) — float32
  ↓ (14) Modality LUT (POST-11) — float32 → uint16
  ↓ (15) VOI LUT (POST-12) — uint16
  ↓ (16) Presentation LUT (POST-13) — uint16 → [GSDF]
  ↓ (17) DICOM Write (SUP-01/02) — uint16
  ↓ DICOM File
```

---

## 3. 상세 데이터 흐름 분석

### 3.1 전처리 단계 (PRE-01~09)

#### 단계별 데이터 흐름

```
[uint16 Raw Frame] 
  ↓
PRE-01: Readout Validation → uint16 (알릿 플래그 설정)
  ↓
PRE-07: Temperature Compensation → uint16 (온도 메타데이터)
  ↓
PRE-02: Offset Correction → uint16 (Dark Current 보정)
  ↓
PRE-08: Nonlinearity Correction → uint16 (선형화)
  ↓
PRE-03: Gain Correction → float32 (Flat Field 보정 + 형식 변환)
  ↓
PRE-09: Binning Correction → float32 (조건부)
  ↓
PRE-06: Defect Correction → float32 (보간)
  ↓
PRE-09: Ghost Correction → float32 [TIER 1/2/3 NLCSC 알고리즘]
```

#### 데이터 구조 변화

```c
// 입력: Raw uint16 Frame
typedef struct {
    uint16_t* data;      // [3072x3072] 픽셀 데이터
    uint32_t width;      // 3072
    uint32_t height;     // 3072  
    uint32_t flags;      // 플래그 비트
} XpeImageBuffer;

// 출력: Gain Correction 후
// 변환: uint16 → float32 + 메타데이터 업데이트
typedef struct {
    float* data;         // [3072x3072] float32 데이터
    uint32_t width;      // 3072
    uint32_t height;     // 3072
    uint32_t flags;      // XPE_FLAG_GAIN_CORRECTED 추가
    float pixelPitch_mm; // 추가 메타데이터
} XpeImageBuffer;
```

### 3.2 형식 변환 체크포인트

#### Checkpoint 1: Gain Correction 후 (PRE-03 → POST-01)

```c
// 형식 변환 함수
XpeErrorCode xpe_gain_correct(XpeImageBuffer* img, const XpeImageBuffer* gainMap) {
    // 1. uint16 → float32 변환
    for (int i = 0; i < img->width * img->height; i++) {
        img->data[i] = (float)img->data[i] / gainMap->data[i];
    }
    
    // 2. 플래그 설정
    img->flags |= XPE_FLAG_GAIN_CORRECTED;
    
    // 3. 데이터 무결성 검증
    ValidateFloat32Buffer(img);
    
    return XPE_OK;
}
```

#### Checkpoint 2: DICOM 출력 전 (POST-13 → SUP-01)

```c
// 형식 변환: float32 → uint16
XpeErrorCode xpe_presentation_lut_apply(XpeImageBuffer* img, const char* presetName) {
    // 1. LUT 적용 (float32 → uint16)
    ApplyLUT(img->data, img->width * img->height, presetName);
    
    // 2. 최종 범위 검증 [0, 65535]
    ValidateOutputRange(img);
    
    // 3. GSDF 준수 검사
    float compliance = CheckGSDFCompliance(img);
    
    return XPE_OK;
}
```

---

## 4. 메타데이터 흐름

### 4.1 메타데이터 구조

```c
typedef struct XpeImageMetadata {
    char     bodyPart[64];      // 신체 부위: "CHEST", "HAND", "SPINE"
    float    kVp;               // 튜브 전압 (kV)
    float    mAs;               // 노출량 (mAs)
    float    SID_mm;            // 원거리 (mm)
    float    pixelPitch_mm;     // 픽셀 피치 (mm)
    uint64_t acquisitionTime;   // 획득 시간 (Unix ms)
    uint32_t flags;            // 처리 상태 플래그
} XpeImageMetadata;
```

### 4.2 플래그 전파 경로

```
Raw Frame → XPE_FLAG_READOUT_VALIDATED (PRE-01)
  ↓
Temperature Compensation → XPE_FLAG_TEMP_COMPENSATED (PRE-07)
  ↓
Gain Correction → XPE_FLAG_GAIN_CORRECTED (PRE-03)
  ↓
Defect Correction → XPE_FLAG_DEFECT_CORRECTED (PRE-06)
  ↓
Ghost Removal → XPE_FLAG_GHOST_CORRECTED (PRE-09)
  ↓
Collimation Detection → XPE_FLAG_COLLIMATION_DETECTED (POST-09)
  ↓
Stitching → XPE_FLAG_STITCHED (POST-02 AI)
  ↓
Bone Suppression → XPE_FLAG_BONE_SUPPRESSED (POST-03 AI)
  ↓
GSVG Failure → XPE_FLAG_GSVG_SKIPPED (GSVG-01)
```

### 4.3 메타데이터 업데이트 예시

```csharp
public class MetadataProcessor {
    public XpeImageMetadata UpdateMetadata(XpeImageMetadata metadata, ProcessingStage stage) {
        switch (stage) {
            case ProcessingStage.GainCorrected:
                metadata.flags |= (uint)XpeFlags.GAIN_CORRECTED;
                metadata.pixelPitch_mm = CalculatePixelPitch(metadata);
                break;
                
            case ProcessingStage.GhostCorrected:
                metadata.flags |= (uint)XpeFlags.GHOST_CORRECTED;
                metadata.acquisitionTime = GetAcquisitionTime();
                break;
                
            case ProcessingStage.CollimationDetected:
                metadata.flags |= (uint)XpeFlags.COLLIMATION_DETECTED;
                // ROI 정보는 sidecar에 저장
                break;
        }
        
        return metadata;
    }
}
```

---

## 5. 상태 관리 흐름

### 5.1 상태 기반 처리 조건

```c
// 조건부 처리 예시
void ProcessFrame(XpeImageBuffer* img, XpeImageMetadata* meta) {
    // 1. 오프셋 보정 - 항상 실행 (MANDATORY)
    xpe_offset_correct(img, offsetMap);
    
    // 2. 비선형 보정 - 조건부 실행 (CONDITIONAL)
    if (!(meta->flags & XPE_FLAG_NONLINEARITY_CORRECTED)) {
        xpe_nonlinearity_correct(img, config);
    }
    
    // 3. 게인 보정 - 항상 실행 (MANDATORY) + 형식 변환
    xpe_gain_correct(img, gainMap); // uint16 → float32
    
    // 4. 고스트 보정 - 조건부 실행 (CONDITIONAL)
    if (exposureHistoryAvailable) {
        xpe_ghost_correct(ghostHandle, img, meta);
    }
}
```

### 5.2 상태 전환 다이어그램

```
[Initial State] 
  ↓
Calibration Data Loaded → State: READY
  ↓
Frame Received → State: PROCESSING
  ↓
Pre-processing Complete → State: PREPROCESSED
  ↓
Enhancement Complete → State: ENHANCED  
  ↓
Display LUT Applied → State: DISPLAY_READY
  ↓
DICOM Complete → State: FINALIZED
```

---

## 6. 버퍼 관리 흐름

### 6.1 메모리 할당 전략

```c
// 버퍼 관리 클래스
public class BufferManager {
    private Dictionary<uint, XpeImageBuffer> bufferPool;
    private object lockObject = new object();
    
    public XpeImageBuffer GetBuffer(uint width, uint height, XpePixelFormat format) {
        uint key = CalculateBufferKey(width, height, format);
        
        lock (lockObject) {
            if (bufferPool.TryGetValue(key, out XpeImageBuffer buffer)) {
                bufferPool.Remove(key);
                return buffer;
            }
        }
        
        // 새 버퍼 할당
        XpeErrorCode result = xpe_alloc_image(width, height, format, out buffer);
        return buffer;
    }
    
    public void ReturnBuffer(XpeImageBuffer buffer) {
        uint key = CalculateBufferKey(buffer.width, buffer.height, buffer.format);
        
        lock (lockObject) {
            if (bufferPool.Count < MAX_POOL_SIZE) {
                bufferPool[key] = buffer;
            } else {
                xpe_free_image(ref buffer);
            }
        }
    }
}
```

### 6.2 스레드 로컬 버퍼

```c
// 스레드별 버퍼 풀
[ThreadStatic]
private static Dictionary<uint, XpeImageBuffer> threadLocalBufferPool;

public XpeImageBuffer GetThreadLocalBuffer(uint width, uint height, XpePixelFormat format) {
    if (threadLocalBufferPool == null) {
        threadLocalBufferPool = new Dictionary<uint, XpeImageBuffer>();
    }
    
    uint key = CalculateBufferKey(width, height, format);
    
    if (threadLocalBufferPool.TryGetValue(key, out XpeImageBuffer buffer)) {
        threadLocalBufferPool.Remove(key);
        return buffer;
    }
    
    xpe_alloc_image(width, height, format, out buffer);
    return buffer;
}
```

---

## 7. 오류 처리 데이터 흐름

### 7.1 오류 전파 경로

```c
// 오류 처리 클래스
public class ErrorHandler {
    public XpeErrorCode ProcessWithErrorHandling(Func<XpeErrorCode> operation, string operationName) {
        try {
            XpeErrorCode result = operation();
            
            if (result != XpeErrorCode.OK) {
                // 1. 오류 알릿 생성
                CreateAlert(operationName, result);
                
                // 2. 디버그 정보 저장
                SaveDebugInfo(operationName, result);
                
                // 3. 대체 처리 시도
                if (TryFallbackOperation(operationName)) {
                    return XpeErrorCode.OK; // 그레이스풀한 실패
                }
            }
            
            return result;
        }
        catch (Exception ex) {
            // 치명적인 오류
            LogFatalError(operationName, ex);
            return XpeErrorCode.ERR_PROCESSING_FAILED;
        }
    }
    
    private bool TryFallbackOperation(string operationName) {
        switch (operationName) {
            case "xpe_ghost_correct":
                // 고스트 보정 실패 시 Tier 다운그레이드
                return DowngradeGhostTier();
                
            case "xpe_stitch_images":
                // 스티칭 실패 시 원본 이미지 반환
                return ReturnOriginalImage();
                
            default:
                return false;
        }
    }
}
```

### 7.2 알릿 큐 관리

```c
// 알릿 시스템
public class AlertManager {
    private ConcurrentQueue<Alert> alertQueue;
    private object queueLock = new object();
    
    public void AddAlert(AlertSeverity severity, string message, string source) {
        var alert = new Alert {
            Severity = severity,
            Message = message,
            Source = source,
            Timestamp = DateTime.UtcNow,
            FrameId = GetCurrentFrameId()
        };
        
        lock (queueLock) {
            alertQueue.Enqueue(alert);
            
            // 크기 제한
            if (alertQueue.Count > MAX_ALERT_QUEUE_SIZE) {
                alertQueue.TryDequeue(out _);
            }
        }
    }
    
    public List<Alert> GetPendingAlerts() {
        var alerts = new List<Alert>();
        
        lock (queueLock) {
            while (alertQueue.TryDequeue(out Alert alert)) {
                alerts.Add(alert);
            }
        }
        
        return alerts;
    }
}
```

---

## 8. 성능 모니터링 데이터 흐름

### 8.1 성능 데이터 수집

```c
// 성능 모니터
public class PerformanceMonitor {
    private Dictionary<string, Stopwatch> stageTimers;
    private Dictionary<string, PerformanceCounter> counters;
    
    public void StartStage(string stageName) {
        stageTimers[stageName] = Stopwatch.StartNew();
    }
    
    public void EndStage(string stageName) {
        if (stageTimers.TryGetValue(stageName, out Stopwatch timer)) {
            timer.Stop();
            
            // 성능 카운터 업데이트
            var counter = counters[stageName];
            counter.Increment(timer.ElapsedMilliseconds);
            
            // 느린 스테이지 감지
            if (timer.ElapsedMilliseconds > SLOW_STAGE_THRESHOLD) {
                AddAlert(AlertSeverity.WARN, $"Slow stage detected: {stageName}", "PERFORMANCE");
            }
        }
    }
    
    public PerformanceReport GenerateReport() {
        var report = new PerformanceReport();
        
        foreach (var kvp in stageTimers) {
            report.StageTimes[kvp.Key] = kvp.Value.ElapsedMilliseconds;
        }
        
        return report;
    }
}
```

### 8.2 메모리 사용량 모니터링

```c
// 메모리 모니터
public class MemoryMonitor {
    public void MonitorPeakUsage() {
        long currentUsage = GetCurrentProcessMemoryUsage();
        
        if (currentUsage > peakUsage) {
            peakUsage = currentUsage;
            PeakMemoryTime = DateTime.UtcNow;
        }
        
        // 메모리 경고
        if (currentUsage > WARNING_THRESHOLD) {
            AddAlert(AlertSeverity.WARN, "High memory usage detected", "MEMORY");
        }
    }
    
    public MemorySnapshot GetCurrentSnapshot() {
        return new MemorySnapshot {
            WorkingSet = Process.GetCurrentProcess().WorkingSet64,
            PrivateMemory = Process.GetCurrentProcess().PrivateMemorySize64,
            PeakVirtualMemory = Process.GetCurrentProcess().PeakVirtualMemorySize64,
            ThreadCount = Process.GetCurrentProcess().Threads.Count,
            HandleCount = Process.GetCurrentProcess().HandleCount
        };
    }
}
```

---

## 9. 데이터 검증 흐름

### 9.1 데이터 무결성 검증

```c
// 검증 클래스
public class DataValidator {
    public XpeErrorCode ValidateImageBuffer(XpeImageBuffer* buffer) {
        // 1. 크기 검증
        if (buffer->width == 0 || buffer->height == 0) {
            return XpeErrorCode.ERR_INVALID_INPUT;
        }
        
        // 2. 데이터 포인터 검증
        if (buffer->data == IntPtr.Zero) {
            return XpeErrorCode.ERR_INVALID_INPUT;
        }
        
        // 3. 데이터 크기 검증
        ulong expectedSize = (ulong)buffer->width * buffer->height * GetPixelSize(buffer->format);
        if (buffer->dataSize < expectedSize) {
            return XpeErrorCode.ERR_BUFFER_TOO_SMALL;
        }
        
        // 4. 픽셀 값 범위 검증
        return ValidatePixelRange(buffer);
    }
    
    private XpeErrorCode ValidatePixelRange(XpeImageBuffer* buffer) {
        switch (buffer->format) {
            case XpePixelFormat.XPE_PIXEL_UINT16:
                return ValidateUint16Range(buffer);
            case XpePixelFormat.XPE_PIXEL_FLOAT32:
                return ValidateFloat32Range(buffer);
            default:
                return XpeErrorCode.ERR_UNSUPPORTED_FORMAT;
        }
    }
}
```

### 9.2 파이프라인 체크포인트

```c
// 체크포인트 관리자
public class PipelineCheckpoint {
    private List<Checkpoint> checkpoints;
    
    public XpeErrorCode ValidateCheckpoint(int checkpointId, XpeImageBuffer* buffer) {
        var checkpoint = checkpoints[checkpointId];
        
        // 1. 데이터 형식 검증
        if (checkpoint.expectedFormat != buffer->format) {
            return XpeErrorCode.ERR_UNSUPPORTED_FORMAT;
        }
        
        // 2. 데이터 범위 검증
        XpeErrorCode rangeResult = ValidateDataRange(buffer, checkpoint.expectedRange);
        if (rangeResult != XpeErrorCode.OK) {
            return rangeResult;
        }
        
        // 3. 메타데이터 검증
        return ValidateMetadata(checkpoint.metadataRequirements);
    }
    
    private XpeErrorCode ValidateDataRange(XpeImageBuffer* buffer, DataRange expected) {
        // 픽셀 값이 예상 범위 내에 있는지 검증
        for (ulong i = 0; i < buffer->dataSize / GetPixelSize(buffer->format); i++) {
            float value = GetPixelValue(buffer, i);
            if (value < expected.min || value > expected.max) {
                return XpeErrorCode.ERR_PROCESSING_FAILED;
            }
        }
        return XpeErrorCode.OK;
    }
}
```

---

## 10. 통합 데이터 흐름 예제

### 10.1 완전한 처리 파이프라인

```csharp
public class XPEDataFlowProcessor {
    private BufferManager bufferManager;
    private MetadataProcessor metadataProcessor;
    private ErrorHandler errorHandler;
    private PerformanceMonitor performanceMonitor;
    
    public DicomImage ProcessRawToDicom(IntPtr rawPixels, AcquisitionParameters acquisition) {
        // 1. 데이터 준비
        XpeImageBuffer inputBuffer = bufferManager.GetBuffer(
            acquisition.Width, acquisition.Height, XpePixelFormat.XPE_PIXEL_UINT16);
        
        CopyRawData(inputBuffer, rawPixels);
        
        // 2. 메타데이터 초기화
        XpeImageMetadata metadata = InitializeMetadata(acquisition);
        
        // 3. 전처리 파이프라인
        PreprocessingPipeline(inputBuffer, ref metadata);
        
        // 4. 향상 처리 파이프라인
        EnhancementPipeline(inputBuffer, ref metadata);
        
        // 5. 디스플레이 처리
        DisplayPipeline(inputBuffer, ref metadata);
        
        // 6. DICOM 생성
        DicomImage dicom = CreateDicomImage(inputBuffer, metadata);
        
        // 7. 리소스 정리
        bufferManager.ReturnBuffer(inputBuffer);
        
        return dicom;
    }
    
    private void PreprocessingPipeline(XpeImageBuffer buffer, ref XpeImageMetadata metadata) {
        performanceMonitor.StartStage("Preprocessing");
        
        try {
            // Readout 검증
            errorHandler.ProcessWithErrorHandling(
                () => ValidateReadoutArtifact(buffer), "Readout Validation");
            
            // 온도 보상
            if (acquisition.Temperature != 25.0f) {
                errorHandler.ProcessWithErrorHandling(
                    () => xpe_temp_compensate(buffer, acquisition.Temperature), "Temperature Compensation");
            }
            
            // 오프셋 보정 (MANDATORY)
            errorHandler.ProcessWithErrorHandling(
                () => xpe_offset_correct(buffer, offsetMap), "Offset Correction");
            
            // 게인 보정 (MANDATORY + Format Boundary)
            errorHandler.ProcessWithErrorHandling(
                () => xpe_gain_correct(buffer, gainMap), "Gain Correction");
            
            // 고스트 보정 (CONDITIONAL)
            if (ShouldApplyGhostCorrection(metadata)) {
                errorHandler.ProcessWithErrorHandling(
                    () => xpe_ghost_correct(ghostHandle, buffer, metadata), "Ghost Correction");
            }
            
        }
        finally {
            performanceMonitor.EndStage("Preprocessing");
        }
    }
    
    private void EnhancementPipeline(XpeImageBuffer buffer, ref XpeImageMetadata metadata) {
        performanceMonitor.StartStage("Enhancement");
        
        try {
            // 로그 변환
            errorHandler.ProcessWithErrorHandling(
                () => xpe_log_transform(buffer, null), "Log Transform");
            
            // 신체 부위 인식 (AI)
            if (aiInitialized) {
                RecognizeBodyPart(buffer, ref metadata);
            }
            
            // 콜리메이션 검출
            if (detectCollimation) {
                DetectCollimation(buffer, ref metadata);
            }
            
            // 노이즈 감소
            errorHandler.ProcessWithErrorHandling(
                () => xpe_noise_reduce(buffer, noiseConfig), "Noise Reduction");
            
            // 대비 향상
            errorHandler.ProcessWithErrorHandling(
                () => xpe_contrast_enhance(buffer, contrastConfig), "Contrast Enhancement");
            
        }
        finally {
            performanceMonitor.EndStage("Enhancement");
        }
    }
}
```

---

## 11. 데이터 흐름 모니터링

### 11.1 실시간 모니터링

```c
// 모니터링 서비스
public class DataFlowMonitor {
    public void StartMonitoring() {
        // 성능 모니터링
        Task.Run(() => MonitorPerformance());
        
        // 메모리 모니터링  
        Task.Run(() => MonitorMemory());
        
        // 오류 모니터링
        Task.Run(() => MonitorErrors());
    }
    
    private void MonitorPerformance() {
        while (isRunning) {
            var report = performanceMonitor.GenerateReport();
            
            // 성능 지표 로깅
            LogPerformanceMetrics(report);
            
            // 느린 스테이지 알릿
            if (report.SlowStages.Count > 0) {
                AddAlert(AlertSeverity.WARN, "Performance degradation detected", "MONITOR");
            }
            
            Task.Delay(1000).Wait();
        }
    }
}
```

### 11.2 데이터 흐름 분석

```c
// 분석 클래스
public class DataFlowAnalyzer {
    public FlowAnalysisResult AnalyzeFlowPatterns() {
        var result = new FlowAnalysisResult();
        
        // 처리 시간 분석
        result.ProcessingTimeDistribution = AnalyzeProcessingTime();
        
        // 메모리 사용 패턴 분석
        result.MemoryUsagePatterns = AnalyzeMemoryUsage();
        
        // 오류 패턴 분석
        result.ErrorPatterns = AnalyzeErrorPatterns();
        
        // 병목 현상 분석
        result.Bottlenecks = IdentifyBottlenecks();
        
        return result;
    }
}
```

---

## 12. 참고 문서

- `.moai/project/pipeline-spec.md` - 파이프라인 상세 명세
- `.moai/project/api-spec.md` - API 참조 문서
- `.moai/project/entry-points.md` - 진입점 문서

---

*최종 업데이트: 2026-04-17*