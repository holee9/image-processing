# Project Interview

**Date**: 2026-04-17
**Project**: XPE (X-ray Processing Engine)
**Type**: Existing Project Update

## Round 1: Ownership and Purpose

**Question**: 이 프로젝트를 유지관리하는 주체이며, 앞으로의 주요 목표는 무엇입니까?

**Answer**: 활성 제품 개발 중 (Active product being developed further)

**Context**:
- This codebase is actively developed
- Documentation should reflect current trajectory and roadmap
- Focus on ongoing development and future enhancements

## Round 2: Constraints and Non-Goals

**Question**: 이 프로젝트에 알려진 제약조건, 기술 부채, 또는 의도적으로 하지 않는 것들이 있습니까?

**Answer**: 특별한 제약조건 없음 (No known critical constraints)

**Context**:
- Document the codebase as-is without constraint annotations
- No critical performance or scalability bottlenecks to document
- No specific security or compliance constraints beyond standard IEC 62304

## Round 3: Documentation Priority

**Question**: 문서에서 가장 정확하게 포착해야 할 중요한 측면은 무엇입니까?

**Answer**: 아키텍처와 모듈 경계 (Architecture and module boundaries)

**Context**:
- Prioritize documenting how the system is structured
- Focus on module interactions and boundaries
- Emphasize the modular DLL architecture and interface contracts
- Document the 3-Layer Anti-Spaghetti architecture pattern

---

## Analysis Findings Summary

**Detected Technologies**:
- Languages: C++17, C11, C# .NET 8
- Build: CMake 3.25+, Ninja, vcpkg
- Testing: Google Test, C# E2E
- Documentation: IEC 62304 Class B compliant

**Architecture Pattern**:
- Modular DLL architecture (7 XPE + 1 GSVG + C# GUI)
- Anti-Spaghetti 3-Layer design
- Strict ABI boundaries (C ABI for P/Invoke)
- Independent module deployment

**Key Modules**:
- Layer 0: xpe_common.dll (infrastructure)
- Layer 1: 7 algorithm DLLs (preprocess, enhance_basic, enhance_advanced, ai, display, dicom, etc.)
- Layer 1-G: gsvg.dll (independent IEC 62304 package)
- Layer 2: ImageProcTest (C# WPF GUI)

**Development Status**:
- Phase 0 complete: Infrastructure, common library, testing framework, GUI prototype, CI/CD
- Phase 1 in progress: Pre/Post-basic algorithms implementation
- Target: Medical device certification (IEC 62304 Class B)
