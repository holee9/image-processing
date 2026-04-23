# Gate G1b → G2 빌드 및 검증 가이드

**빠른 시작**: PowerShell 터미널에서 다음 명령을 실행하세요.

## 1단계: 빌드 실행 (PowerShell)

```powershell
# 프로젝트 루트로 이동
cd D:\workspace-github\image-processing

# 빌드 스크립트 실행 (Clean 빌드)
.\tools\ci\Build-DicomModule.ps1 -Clean
```

## 2단계: 빌드 결과 확인

```powershell
# DLL 의존성 확인
dumpbin /dependents build\ci-common\bin\xpe_dicom.dll

# 예상 출력:
# dcmtk.dll (또는 DCMTK 라이브러리)
# msmpi.dll (MPI 라이브러리, Network SCU용)
# KERNEL32.dll, MSVCP*.dll, VCRUNTIME*.dll (시스템 라이브러리)
```

## 3단계: 테스트 실행

```powershell
# CTest를 통한 전체 테스트 실행
ctest --test-dir build\ci-common --output-on-failure --build-config RelWithDebInfo -V
```

## 4단계: 통합 테스트 실행

```powershell
# ImageProcTest E2E fixture 실행
dotnet build clients\ImageProcTest\ImageProcTest.csproj -c RelWithDebInfo
dotnet clients\ImageProcTest\bin\RelWithDebInfo\net8.0-windows\ImageProcTest.dll --run-preprocess-fixture-e2e
```

## 예상 실행 시간

- CMake 설정: ~2분
- 빌드: ~5분
- 단위 테스트: ~2분
- 통합 테스트: ~3분
- **총**: ~12분

## 문제 해결

### DCMTK를 찾을 수 없음

```powershell
# vcpkg로 DCMTK 설치
vcpkg install dcmtk:x64-windows
vcpkg install openjpeg:x64-windows
```

### 빌드 오류: LINK fatal error LNK1104

```powershell
# Visual Studio 환경 변수 재설정
# "Developer Command Prompt for VS 2022"에서 실행
```

### 테스트 데이터 누락

```powershell
# E2E fixture가 테스트 데이터를 자동 생성합니다
# --run-preprocess-fixture-e2e --generate-test-data
```

---

## 성능 검증 (Gate G1b → G2)

### E2E Latency 측정

```powershell
# 3072×3072 Raw DICOM → E2E 타이밍 측정
dotnet clients\ImageProcTest\bin\RelWithDebInfo\net8.0-windows\ImageProcTest.dll `
  --run-preprocess-fixture-e2e `
  --input "testdata\3072x3072_raw.dcm" `
  --measure-latency `
  --iterations 100
```

### Memory Profiling

```powershell
# 1000프레임 연속 처리 메모리 프로파일링
dotnet clients\ImageProcTest\bin\RelWithDebInfo\net8.0-windows\ImageProcTest.dll `
  --run-preprocess-fixture-e2e `
  --frames 1000 `
  --measure-memory
```

---

## Gate 통과 기준

| 항목 | 기준 | 확인 방법 |
|------|------|----------|
| 단위 테스트 | 403/403 PASS | ctest 출력 |
| 통합 테스트 | 78/78 PASS | ImageProcTest 출력 |
| E2E Latency | < 3000ms | E2E fixture 타이밍 리포트 |
| Peak Memory | <= 190MB | 메모리 프로파일 리포트 |

---

## 다음 작업

빌드 및 검증 완료 후:

1. **Gate 통과**: dev-plan.md에 M1 완료 표시
2. **점수 갱신**: Framework A +2점 반영
3. **다음 작업**: SPEC-SIMD-001 Pre-A 실구현 (임계경로)

---

**참고 문서**:
- `docs/development/Gate-G1b-G2-Verification-Plan.md` - 상세 검증 계획
- `tools/ci/Build-DicomModule.ps1` - 빌드 스크립트 소스
- `.moai/project/dev-plan.md` - 프로젝트 진행 상황
