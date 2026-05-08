# Session 10 Brief — xpe-pre Worktree (Pre-A Lane)

**작성일**: 2026-05-08
**워크트리**: `D:/workspace-github/image-processing-preprocess`
**브랜치**: `feat/preprocessing`
**소유 모듈**: `modules/preprocess`, `modules/common`(공유 한정)
**기준 감사**: `docs/calibration/PREPROCESS-AUDIT-SESSION9.md`, `SPEC-XPE-P1A-QUALITY-REPORT.md`

---

## 1. 즉시 정리 — 현재 WIP 커밋/푸시

본 워크트리에 13건의 미커밋 변경이 있습니다. **신규 작업 시작 전 본 WIP를 의미 단위로 분할 커밋/푸시하여 정리한 뒤 새 작업 진입**.

### 1.1 코드 변경 (modules/preprocess/src)

```
M  modules/preprocess/src/offset_correct.cpp
M  modules/preprocess/src/xpe_calib_generate_offset.cpp
?? modules/preprocess/src/xpe_calib_generate_offset_methods.cpp   (신규)
?? modules/preprocess/src/xpe_calib_generate_offset_methods.hpp   (신규)
M  modules/preprocess/src/xpe_calib_mode.cpp
M  modules/preprocess/src/xpe_defect_gen.cpp
M  modules/preprocess/src/xpe_verify_metrics.cpp
```

권장 커밋 분할:
- `feat(pre): calibration multi-method 분리 — generate_offset_methods` (#69 P0-CAL-01)
- `fix(pre): offset_correct SIMD dispatch 활성화 + 라운딩 통일` (#68 P0-OFF-01/02)
- `fix(pre): defect_gen reflect padding + xpe_verify_metrics 보강` (#70 관련)

### 1.2 테스트 변경 (modules/preprocess/tests)

```
?? modules/preprocess/tests/test_calib_generate_offset_multi.cpp  (신규)
M  modules/preprocess/tests/test_defect_avx2_parity.cpp
M  modules/preprocess/tests/test_gain_avx2_parity.cpp
M  modules/preprocess/tests/test_offset_avx2_parity.cpp
```

권장 커밋:
- `test(pre): AVX2 parity GTEST_SKIP 해제 + multi-method 테스트` (#68 P1-OFF-04, #69 P0-CAL-01)

### 1.3 문서 변경 (docs/calibration)

```
M  docs/calibration/ALGORITHM-VERIFICATION-GUIDE.md
M  docs/calibration/PREPROCESS-AUDIT-SESSION9.md
M  docs/calibration/RTM-CALIB-001.md
```

권장 커밋: `docs(calibration): 세션 9 감사 결과 + RTM 갱신`

### 1.4 푸시 후 PR 정책

- 각 커밋이 정상 빌드/테스트 통과 후 `git push origin feat/preprocessing`
- 본 Brief의 신규 작업 PR과 별도로 분리 머지 (revert 가능성 보존)

---

## 2. 신규 작업 — Must (P0, 출시 블로커)

본 워크트리의 P0 작업 4건은 모두 GitHub Issue로 추적됩니다. WIP 정리 후 다음 순서로 마무리.

### 2.1 #68 — Offset 이중 구현 제거 + SIMD dispatch (P0-OFF-01/02)

**범위**: `modules/preprocess/src/{offset_correct.cpp, xpe_offset.cpp, gain_correct.cpp, xpe_gain.cpp}`

수용 기준 (#68 본문 요약):
- [ ] `xpe_offset_correct` 심볼이 단일 파일에만 정의
- [ ] 라운딩 정책 통일 (+0.5f 반올림 또는 절삭, SPEC 명시)
- [ ] SIMD dispatch가 공개 API에서 활성화 (AVX2/AVX-512/NEON)
- [ ] `test_offset_correct.cpp` 3-arg API로 전환
- [ ] AVX2 parity 테스트 GTEST_SKIP 해제 (offset/gain/defect 3종)
- [ ] Gain null 게인맵 동작 통일 (`xpe_gain.cpp` pass-through 제거 또는 `gain_correct.cpp`로 흡수)

### 2.2 #69 — Calibration multi-method + Cache 스레드 안전성 (P0-CAL-01/02)

**범위**: `modules/preprocess/src/{xpe_calib_generate_offset.cpp, xpe_calib_generate_offset_methods.{cpp,hpp}, calibration_cache.cpp, xpe_calib_mode.cpp, xpe_calib_load_defect_map.cpp}`

수용 기준 (#69 본문):
- [ ] `method` 파라미터 파싱 (mean/median/sigma_clip/winsor)
- [ ] median, sigma_clip, winsor 통계 함수 구현 + 테스트 GREEN
- [ ] `CalibrationLRUCache`에 `std::mutex` 추가, 4-thread endurance 테스트 통과
- [ ] Defect map 만료 정책 결정 + 문서화
- [ ] R² 회귀 감지 시 spdlog 로깅 구현 (FUNC-033)
- [ ] Polynomial gain 테스트 실패 허용 제거

### 2.3 #70 — Defect bilinear + Hampel + Reflect padding (P0-DEF-01)

**범위**: `modules/preprocess/src/{defect_correct.cpp, xpe_defect_gen.cpp, runtime_detection.cpp}`

수용 기준 (#70 본문):
- [ ] `xpe_interpolate_pixel()` 구현 확인 + 누락 시 구현
- [ ] Reflect padding 공식 검증 (xpe_defect_gen.cpp:179) + 수정
- [ ] Hampel 5-sigma 검출 구현 + 테스트 활성화
- [ ] BPM merge를 explicit 매핑으로 변경
- [ ] 8개 이웃 모두 결함 시 fallback 개선 (defect_correct.cpp:95)
- [ ] 코너/경계/전체 결함 클러스터 엣지 케이스 테스트 추가

### 2.4 #73 — P1A 심각 결함 4건 + REQ-P1A-066 (신규 등록)

**범위**: `modules/preprocess/src/{gain_correct.cpp, calibration_manager.cpp, ghost_*.cpp}` + 오류 경로 테스트

수용 기준 (#73 본문):
- [ ] D1: `gain_correct.cpp:37-45` malloc NULL 검사 + 도메인 전환 경로 보강 → `XPE_ERR_OOM`
- [ ] D2: `calibration_manager.cpp` fopen↔fclose RAII 또는 모든 분기 fclose
- [ ] D3: 모든 `xpe_ghost_*` public API 진입부 magic 검증 + 단위 테스트
- [ ] D4: `stride * height` 곱셈 전 SIZE_MAX/height 비교 (gain/offset/defect)
- [ ] REQ-P1A-066: malloc 실패 / fopen 권한 거부 / CRC 불일치 / Ghost race 4종 단위 테스트
- [ ] `SPEC-XPE-P1A-QUALITY-REPORT.md` 결함 항목에 종결 날짜 기재

---

## 3. 신규 작업 — Should (S1, 점수 상승)

### 3.1 #57 — SPEC-SIMD-001 Scalar Reference + AVX2 Parity 검증

**범위**: `modules/preprocess/src/{gain,defect,runtime_detection}*.cpp` + SIMD parity 테스트

#68/#69/#70/#73 완료 후 진행. SIMD-001 스칼라 기준 + AVX2 출력이 비트 동일성(또는 ULP=0) 만족 확인. Framework A +5점, Framework B +3점 효과.

수용 기준 (#57 본문 + 본 세션 추가):
- [ ] `runtime_detection.cpp`의 모든 dispatch 분기에서 스칼라 fallback 등록
- [ ] 3종 (gain/offset/defect) AVX2 parity 테스트 ULP=0 통과
- [ ] AVX-512 path 활성 시 동일 검증 추가
- [ ] BP-01~05 벤치마크에서 SIMD 적용 후 성능 측정 (`< 15ms @ 3072×3072`)
- [ ] SPEC-SIMD-001 RTM에 본 PR 링크 추가

---

## 4. Done Definition (xpe-pre, 세션 10)

본 워크트리의 세션 10 종결 조건:

- [ ] WIP 13건 정리 (3-4 커밋 분할 + push)
- [ ] #68/#69/#70/#73 모두 PR 생성 + 머지 + Issue 자동 종결
- [ ] #57 SIMD-001 검증 통과 (또는 후속 세션 이관 사유 명시)
- [ ] 403/403 + 신규 4종 + AVX2 parity 모두 GREEN
- [ ] `git diff --check` clean
- [ ] CI MSVC /W4 /WX 통과 (C4365/C4244/C4505 신규 발생 0건)
- [ ] `SPEC-XPE-P1A-QUALITY-REPORT.md` 갱신 (4건 결함 종결 날짜)
- [ ] 본 Brief 하단에 완료 보고 추가

---

## 5. 빌드 및 검증 명령

```powershell
# WIP 정리 직후
cd D:\workspace-github\image-processing-preprocess
cmake --build build --config Release --target test_preprocess
ctest --test-dir build -C Release --output-on-failure -R "preprocess|calib|defect|offset|gain"

# AVX2 parity 검증
ctest --test-dir build -C Release -R "avx2_parity"

# CRC 무결성 (REQ-P1A-066)
ctest --test-dir build -C Release -R "endurance|race|crc"
```

---

## 6. 참조

- 본 Brief: 세션 10차 작업 분배 (오케스트레이터: main, 2026-05-08)
- SPEC: SPEC-XPE-P1A, SRS-CALIB-001, SPEC-SIMD-001
- 이슈: #57, #68, #69, #70, #73
- 감사 보고서: `docs/calibration/PREPROCESS-AUDIT-SESSION9.md`
- 품질 보고서: 워크트리 루트 `SPEC-XPE-P1A-QUALITY-REPORT.md`

---

## 7. 진행 기록 (작업자 갱신)

작업 진행 시 본 섹션에 일자별 코멘트 추가 (`pre:` 프리픽스 사용):

```
2026-05-08 pre: WIP 분할 커밋 시작
```

(공란)
