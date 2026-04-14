# XPE 프로덕션 소프트웨어 통합 가이드

**Document ID**: XPE-PROD-INT-001  
**Version**: 1.0.0  
**Date**: 2026-04-14  
**Purpose**: RadiConsole 등 프로덕션 의료 소프트웨어가 XPE 라이브러리를 통합할 때 필요한 경로 관리 및 배포 규약  
**Audience**: 의료기기 SW 개발팀 (XPE 라이브러리 사용자)

---

## 개요

XPE 라이브러리의 C ABI는 **명시적 파일 경로 방식 (explicit path API)**을 사용합니다. 즉, 라이브러리가 고정된 기본 데이터 디렉터리를 가정하지 않고, **호출자(프로덕션 SW)가 모든 경로를 명시적으로 제공**합니다.

```
ProDSW (RadiConsole)
  │
  ├─ xpe_dicom_read("C:\\clinical\\data\\patient_001.dcm", ...)
  ├─ xpe_calib_load_offset("C:\\calib\\offset_map_2024.xpe_calib", ...)
  ├─ xpe_calib_load_gain("C:\\calib\\gain_map_2024.xpe_calib", ...)
  ├─ xpe_calib_load_defect_map("D:\\calib\\defect_map.xpe_calib", ...)
  ├─ xpe_ai_init("C:\\models\\xpe_ai_models\\", ...)
  └─ gsvg_load_scatter_lut("C:\\calib\\gsvg\\grid_lut.dat", ...)
        ↓
    XPE DLLs (no internal path assumptions)
```

이 문서는 프로덕션 SW가 이러한 경로들을 **일관되게 관리하고 배포**하기 위한 권장 규약을 정의합니다.

---

## 1. 데이터 디렉터리 배포 구조

### 1.1 설치 시 디렉터리 레이아웃

프로덕션 SW(예: RadiConsole)를 설치할 때 다음 구조로 XPE 데이터를 배포하십시오:

```
C:\Program Files\RadiConsole\          # 설치 root
├── RadiConsole.exe
├── xpe_common.dll                     # XPE Layer 0
├── xpe_preprocess.dll
├── xpe_enhance_basic.dll
├── xpe_enhance_advanced.dll
├── xpe_display.dll
├── xpe_dicom.dll
├── xpe_ai.dll
├── xpe_ai_worker.exe
├── gsvg.dll
│
├── data/                              # **XPE 공유 데이터 디렉터리**
│   ├── calibration/                   # 캘리브레이션 파일 (환자 독립적)
│   │   ├── offset_map_*.xpe_calib
│   │   ├── gain_map_*.xpe_calib
│   │   ├── defect_map_*.xpe_calib
│   │   └── gsvg/
│   │       └── grid_lut.dat           # GSVG scatter LUT
│   │
│   ├── models/                        # AI 모델 파일
│   │   ├── bodypart_mobilenet_v3.onnx (~80MB)
│   │   ├── bone_suppression_unet.onnx (~200MB)
│   │   └── denoiser/
│   │       ├── denoiser_chest_100mAs.onnx
│   │       ├── denoiser_chest_50mAs.onnx
│   │       └── ...
│   │
│   ├── lut/                           # 조회 테이블
│   │   ├── dicom_grayscale_pipeline.lut
│   │   ├── body_part_lookup.json
│   │   └── exposure_index_reference.json
│   │
│   └── config/                        # 고정 설정 (설치 후 드물게 변경)
│       └── xpe_default_config.json    # 기본 설정 템플릿
│
├── user_data/                         # **환자 및 임상 데이터** (선택적)
│   ├── processed_images/              # 처리된 DICOM 출력
│   └── calibration_records/           # 캘리브레이션 이력
│
└── logs/                              # **런타임 로그**
    └── xpe_runtime.log
```

### 1.2 관리 권한

| 디렉터리 | 소유자 | 읽기 | 쓰기 | 목적 |
|---------|-------|------|------|-----|
| `data/calibration/` | IT 관리자 | RadiConsole | (배포 시에만) | 캘리브레이션 데이터 (거의 변경 없음) |
| `data/models/` | IT 관리자 | RadiConsole | (배포 시에만) | AI 모델 (고정) |
| `data/lut/` | IT 관리자 | RadiConsole | (배포 시에만) | LUT 데이터 (고정) |
| `data/config/` | 임상 운영자 | RadiConsole | ✅ | 런타임 설정 (사이트별 커스터마이징) |
| `user_data/` | RadiConsole | RadiConsole | ✅ | 환자 DICOM, 처리 결과 |
| `logs/` | RadiConsole | RadiConsole | ✅ | 런타임 로그 (감사 추적) |

---

## 2. 경로 관리 패턴 (C# / .NET)

### 2.1 초기화 시 경로 설정

ProDSW는 시작 시 XPE 경로를 설정해야 합니다:

```csharp
using System.IO;

public class XpePathManager
{
    private readonly string _installRoot;
    private readonly string _dataDir;
    
    public XpePathManager(string installRoot = null)
    {
        _installRoot = installRoot ?? Path.GetDirectoryName(Application.ExecutablePath);
        _dataDir = Path.Combine(_installRoot, "data");
    }
    
    // 경로 속성 (읽기 전용)
    public string CalibrationDir => Path.Combine(_dataDir, "calibration");
    public string OffsetCalibPath => Path.Combine(CalibrationDir, GetLatestCalib("offset_map_*.xpe_calib"));
    public string GainCalibPath => Path.Combine(CalibrationDir, GetLatestCalib("gain_map_*.xpe_calib"));
    public string DefectMapPath => Path.Combine(CalibrationDir, GetLatestCalib("defect_map_*.xpe_calib"));
    
    public string AiModelDir => Path.Combine(_dataDir, "models");
    public string GsvgLutPath => Path.Combine(CalibrationDir, "gsvg", "grid_lut.dat");
    
    public string UserDataDir => Path.Combine(_installRoot, "user_data");
    public string ProcessedImagesDir => Path.Combine(UserDataDir, "processed_images");
    
    public string LogDir => Path.Combine(_installRoot, "logs");
    public string LogFilePath => Path.Combine(LogDir, "xpe_runtime.log");
    
    /// <summary>
    /// 환자 DICOM 파일 경로 (동적 - 임상 운영자가 선택)
    /// </summary>
    public string GetPatientDicomPath(string patientId, string fileName)
    {
        return Path.Combine(ProcessedImagesDir, patientId, fileName);
    }
    
    private string GetLatestCalib(string pattern)
    {
        var files = Directory.GetFiles(CalibrationDir, pattern);
        if (files.Length == 0)
            throw new FileNotFoundException($"Calibration file not found: {pattern}");
        
        // 최신 파일 반환 (파일명에 날짜가 포함된다고 가정)
        return Path.GetFileName(files.OrderByDescending(f => File.GetLastWriteTime(f)).First());
    }
}
```

### 2.2 XPE 초기화 (경로 포함)

```csharp
public class XpeInitializer
{
    public static void InitializeXpe(XpePathManager pathMgr)
    {
        // 1. 기본 초기화
        var initConfig = new
        {
            logLevel = 2,  // INFO
            logFile = pathMgr.LogFilePath,
            threadPoolSize = 0  // auto
        };
        
        string initJson = JsonConvert.SerializeObject(initConfig);
        int result = XpeInterop.xpe_init(initJson);
        if (result != XPE_OK)
            throw new InvalidOperationException($"xpe_init failed: {result}");
        
        // 2. 캘리브레이션 로드 (경로 명시적 제공)
        XpeImageBuffer offsetMap = new();
        XpeImageBuffer gainMap = new();
        
        result = XpeInterop.xpe_calib_load_offset(pathMgr.OffsetCalibPath, ref offsetMap);
        if (result != XPE_OK)
            throw new InvalidOperationException($"Failed to load offset calib: {result}");
        
        result = XpeInterop.xpe_calib_load_gain(pathMgr.GainCalibPath, ref gainMap);
        if (result != XPE_OK)
            throw new InvalidOperationException($"Failed to load gain calib: {result}");
        
        // 3. AI 모델 초기화 (경로 제공)
        var aiConfig = new { device = "CPU", timeout_ms = 30000 };
        string aiJson = JsonConvert.SerializeObject(aiConfig);
        result = XpeInterop.xpe_ai_init(pathMgr.AiModelDir, aiJson);
        if (result != XPE_OK)
            Console.WriteLine($"Warning: AI initialization failed (will use classical pipeline): {result}");
        
        // 4. GSVG LUT 로드
        if (File.Exists(pathMgr.GsvgLutPath))
        {
            result = GsvgInterop.gsvg_load_scatter_lut(pathMgr.GsvgLutPath);
            if (result != GSVG_OK)
                Console.WriteLine($"Warning: GSVG LUT load failed: {result}");
        }
    }
}
```

### 2.3 DICOM 처리 (동적 환자 경로)

```csharp
public class DicomProcessor
{
    private readonly XpePathManager _pathMgr;
    
    public void ProcessPatientDicom(string sourceFile, string patientId)
    {
        // 소스 파일 읽기
        var dicomImg = new XpeImageBuffer();
        var dicomMeta = new XpeImageMetadata();
        
        int result = XpeInterop.xpe_dicom_read(sourceFile, ref dicomImg, ref dicomMeta);
        if (result != XPE_OK)
            throw new InvalidOperationException($"Failed to read DICOM: {result}");
        
        // XPE 파이프라인 실행... (생략)
        
        // 처리된 DICOM 저장 (출력 경로는 동적 구성)
        string outputDir = _pathMgr.ProcessedImagesDir;
        Directory.CreateDirectory(Path.Combine(outputDir, patientId));
        
        string outputFile = _pathMgr.GetPatientDicomPath(
            patientId, 
            $"{Path.GetFileNameWithoutExtension(sourceFile)}_processed.dcm"
        );
        
        result = XpeInterop.xpe_dicom_write(outputFile, ref dicomImg, ref dicomMeta, null);
        if (result != XPE_OK)
            throw new InvalidOperationException($"Failed to write DICOM: {result}");
    }
}
```

---

## 3. xpe_init 설정 JSON 스키마 확장

현재 `xpe_init` config에는 데이터 경로가 없습니다. **권장: 향후 패치에서 추가**

```json
{
  "logLevel": 2,
  "logFile": "C:\\Program Files\\RadiConsole\\logs\\xpe_runtime.log",
  "threadPoolSize": 0,
  
  "dataDirectories": {
    "calibrationDir": "C:\\Program Files\\RadiConsole\\data\\calibration",
    "modelDir": "C:\\Program Files\\RadiConsole\\data\\models",
    "lutDir": "C:\\Program Files\\RadiConsole\\data\\lut"
  },
  
  "preprocess": {
    "readout_validation": { "enabled": true },
    "temp_compensation": { "enabled": true },
    "ghost_correction": { "enabled": true, "max_tier": 3 }
  }
}
```

**현재 (v1.0)**: 이 필드들이 없으므로, **호출자가 명시적으로 각 함수에 경로를 전달**해야 합니다.

---

## 4. 배포 체크리스트

프로덕션 설치 시:

- [ ] **DLL 배치**: 모든 7개 XPE DLL + xpe_ai_worker.exe를 설치 root에 배치
- [ ] **데이터 디렉터리 구조**: `data/calibration/`, `data/models/`, `data/lut/` 생성
- [ ] **캘리브레이션 파일**: 최신 offset/gain/defect 파일을 `data/calibration/`에 배치
  - 파일명 규약: `offset_map_YYYY-MM-DD.xpe_calib` (날짜 포함 권장)
- [ ] **AI 모델**: ONNX 모델 파일을 `data/models/`에 배치 (~300MB 총용량)
  - 모델 누락 시 시스템이 classical pipeline으로 자동 fallback 되므로 선택사항이지만 권장
- [ ] **GSVG LUT**: `data/calibration/gsvg/grid_lut.dat` 배치 (선택사항)
- [ ] **로그 디렉터리**: `logs/` 디렉터리 생성 및 쓰기 권한 확인
- [ ] **경로 관리 코드**: ProDSW에 `XpePathManager` 같은 경로 관리 클래스 구현
- [ ] **초기화 코드**: ProDSW 시작 시 모든 경로를 기반으로 XPE 초기화
- [ ] **에러 처리**: 캘리브레이션/모델 파일 누락 시 graceful degradation (classical pipeline)

---

## 5. 사이트별 커스터마이징

### 5.1 설정 오버라이드

임상 사이트가 기본 설정을 변경해야 할 경우:

1. `data/config/site_config.json` 파일 생성
2. ProDSW가 시작 시 이 파일을 읽고 `xpe_configure`로 적용

```csharp
// ProDSW startup
string siteConfigFile = Path.Combine(_pathMgr.ConfigDir, "site_config.json");
if (File.Exists(siteConfigFile))
{
    string configJson = File.ReadAllText(siteConfigFile);
    int result = XpeInterop.xpe_configure(configJson);
    if (result != XPE_OK)
        Console.WriteLine($"Warning: Site config failed to apply: {result}");
}
```

### 5.2 캘리브레이션 업데이트

새로운 캘리브레이션 데이터를 배포할 때:

1. `data/calibration/offset_map_YYYY-MM-DD.xpe_calib` 추가
2. `data/calibration/gain_map_YYYY-MM-DD.xpe_calib` 추가
3. ProDSW의 경로 관리 로직이 **최신 파일을 자동으로 감지** (GetLatestCalib 함수 참조)

---

## 6. 문제 해결

| 증상 | 원인 | 해결 |
|------|------|------|
| `XPE_ERR_IO_FAILED` when loading calibration | 파일 경로 잘못됨 | 경로 존재 여부 확인: `File.Exists(calibPath)` |
| `XPE_ERR_CALIBRATION_EXPIRED` | 캘리브레이션 만료 | 최신 캘리브레이션 파일로 교체 |
| AI 초기화 실패 | 모델 파일 누락 | fallback path가 자동으로 동작 (classical pipeline 사용) |
| 로그 파일 쓰기 실패 | `logs/` 디렉터리 권한 | 디렉터리 쓰기 권한 확인, 디렉터리 미존재 시 생성 |

---

## 7. 참고 문서

- **api-spec.md**: 모든 API 함수 및 매개변수 (filePath 매개변수 참조)
- **xpe-implementation-reference.md**: 캘리브레이션 파일 바이너리 형식
- **xray-milestone-uat-plan.md**: M3 이상의 UAT 절차 (실제 X-ray 파일 사용)

---

**Document End -- XPE-PROD-INT-001 v1.0.0**
