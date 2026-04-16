---
name: xpe-orchestrator
description: "XPE 이미지 처리 엔진 개발 오케스트레이터. xpe_preprocess/enhance_basic/enhance_advanced/ai/display/dicom/gsvg 모듈 구현, Ghost correction/Gain-Offset/CLAHE 알고리즘, Google Test 작성, IEC 62304 문서화를 에이전트 팀으로 조율. 'XPE 모듈 만들어줘', '모듈 구현', '알고리즘 구현', '테스트 작성', 'IEC 문서', '다시 실행', '재실행', '업데이트', '보완', '이전 결과 기반으로' 요청 시 반드시 트리거."
---

# XPE Orchestrator

XPE X-ray 이미지 처리 엔진 개발을 파이프라인 + 팬아웃 하이브리드 패턴으로 조율한다.

## 파이프라인 구조

```
Phase A: xpe-architect (단독)
    → API 헤더 + CMakeLists.txt 설계
    ↓
Phase B: xpe-implementer + xpe-algorithm (팀, 병렬)
    → C++ 소스 + 알고리즘 구현
    ↓
Phase C: xpe-qa (단독)
    → Google Test 작성
    ↓
Phase D: xpe-docs (단독)
    → IEC 62304 문서 갱신
```

## Phase 0: 컨텍스트 확인

워크플로우 시작 시 기존 산출물 존재 여부 확인:

1. `_workspace/` 디렉토리 존재 여부 확인
2. 사용자 요청 분석:
   - `_workspace/` 없음 → **초기 실행** (전체 파이프라인)
   - `_workspace/` 있음 + 부분 수정 요청 → **부분 재실행** (해당 에이전트만)
   - `_workspace/` 있음 + 새 모듈 요청 → **새 실행** (기존 `_workspace/`를 `_workspace_prev/`로 이동 후 시작)

## Phase A: xpe-architect 실행 (서브 에이전트)

**실행 모드**: 단일 서브 에이전트

```
Agent(
    subagent_type: "general-purpose",
    model: "opus",
    prompt: """
    당신은 xpe-architect입니다. .claude/agents/xpe/xpe-architect.md를 읽고 역할을 수행하세요.
    사용할 스킬: xpe-module-impl
    
    요청: {모듈 이름} 모듈의 API 헤더와 CMakeLists.txt를 설계하세요.
    참조: docs/project/api-spec.md, docs/project/structure.md
    출력: modules/{name}/include/..., modules/{name}/CMakeLists.txt
           _workspace/01_architect_{module}_design.md
    """
)
```

**완료 조건**: `_workspace/01_architect_{module}_design.md` 파일 생성

## Phase B: xpe-implementer + xpe-algorithm 병렬 팀

**실행 모드**: 에이전트 팀 (2명 병렬)

```python
# xpe-implementer
Agent(
    subagent_type: "general-purpose",
    model: "opus",
    prompt: """
    당신은 xpe-implementer입니다. .claude/agents/xpe/xpe-implementer.md를 읽고 역할을 수행하세요.
    사용할 스킬: xpe-module-impl
    
    _workspace/01_architect_{module}_design.md를 먼저 읽으세요.
    {모듈} C++ 소스 파일을 구현하세요.
    출력: modules/{name}/src/{name}.cpp
          _workspace/02_implementer_{module}_notes.md
    """
)

# xpe-algorithm (병렬)
Agent(
    subagent_type: "general-purpose", 
    model: "opus",
    prompt: """
    당신은 xpe-algorithm입니다. .claude/agents/xpe/xpe-algorithm.md를 읽고 역할을 수행하세요.
    사용할 스킬: xpe-algorithm
    
    {알고리즘 목록} 구현: scalar reference → SIMD 최적화 순서.
    출력: _workspace/02_algorithm_{module}_notes.md
    """
)
```

**완료 조건**: `_workspace/02_implementer_*` + `_workspace/02_algorithm_*` 모두 생성

## Phase C: xpe-qa (서브 에이전트)

**실행 모드**: 단일 서브 에이전트

```
Agent(
    subagent_type: "general-purpose",
    model: "opus", 
    prompt: """
    당신은 xpe-qa입니다. .claude/agents/xpe/xpe-qa.md를 읽고 역할을 수행하세요.
    사용할 스킬: xpe-testing
    
    _workspace/02_*.md 파일들을 읽고 Google Test를 작성하세요.
    출력: tests/{module}_tests/CMakeLists.txt
          tests/{module}_tests/test_{module}_abi.cpp
          tests/{module}_tests/test_{module}_algorithm.cpp
          _workspace/03_qa_{module}_report.md
    """
)
```

## Phase D: xpe-docs (서브 에이전트)

**실행 모드**: 단일 서브 에이전트

```
Agent(
    subagent_type: "general-purpose",
    model: "opus",
    prompt: """
    당신은 xpe-docs입니다. .claude/agents/xpe/xpe-docs.md를 읽고 역할을 수행하세요.
    사용할 스킬: xpe-iec62304
    
    _workspace/03_qa_{module}_report.md를 읽고 IEC 62304 문서를 갱신하세요.
    출력: docs/project/ 갱신
          _workspace/04_docs_{module}_changelog.md
    """
)
```

## 데이터 전달 규칙

| 단계 | 전달 방식 | 파일명 규칙 |
|------|---------|-----------|
| A → B | 파일 기반 | `_workspace/01_architect_{module}_design.md` |
| B → C | 파일 기반 | `_workspace/02_{agent}_{module}_notes.md` |
| C → D | 파일 기반 | `_workspace/03_qa_{module}_report.md` |
| D → 완료 | 파일 기반 | `_workspace/04_docs_{module}_changelog.md` |

## 에러 핸들링

- Phase A 실패: 1회 재시도 후 사용자에게 api-spec.md 불명확 사항 보고
- Phase B 실패: implementer/algorithm 개별 재시도 가능 (독립 실행)
- Phase C 실패: QA 없이 Phase D 진행 가능 (report에 "QA 미완료" 명시)
- Phase D 실패: 1회 재시도, 실패 시 수동 문서 갱신 안내

## 부분 재실행 패턴

```
사용자: "알고리즘만 다시 구현해줘"
→ Phase B의 xpe-algorithm만 재실행
→ 기존 _workspace/02_implementer_* 유지
→ Phase C, D는 필요 시 재실행

사용자: "테스트 케이스 추가해줘"
→ Phase C (xpe-qa)만 재실행
→ 기존 구현 파일 유지

사용자: "IEC 문서 업데이트해줘"
→ Phase D (xpe-docs)만 재실행
```

## 테스트 시나리오

### 정상 흐름
```
사용자: "xpe_preprocess 모듈 구현해줘"
1. _workspace/ 없음 확인 → 초기 실행
2. xpe-architect → preprocess API 헤더 생성
3. xpe-implementer + xpe-algorithm 병렬 → cpp 구현
4. xpe-qa → Google Test 작성
5. xpe-docs → api-spec.md 갱신
```

### 에러 흐름
```
사용자: "다시 실행해줘"
1. _workspace/ 존재 확인
2. 사용자에게 전체/부분 선택 요청
3. 선택에 따라 해당 Phase만 재실행
```
