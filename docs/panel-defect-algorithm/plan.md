1. 목표 및 범위
이 R&D 계획은 X-ray 플랫-패널 방사선 촬영용 CPU 전용, DLL 기반 이미지 처리 라이브러리를 정의합니다:

결함 검출 알고리즘 구현:

최소 5×5 크기까지의 불량 픽셀 및 클러스터 결함 검출.

1~5 픽셀의 연속 폭을 가진 라인 결함 검출 (좁은 방향).

anti-scatter grid에서 발생하는 그리드/모아레 아티팩트 검출 및 정량화.

Min, Normal, Max의 세 프로필에서 결정 생성.

결함 보정 알고리즘 구현:

3×3 및 5×5 블록의 모든 인접 불량 픽셀을 CPU 실행 가능한 방법으로 보정.

1~5 픽셀 폭의 연속 라인 결함을 보정 유형 1, 3, 5로 보정 (우선순위 및 완성도: 1 > 3 > 5, 유형 1과 3은 필수).

그리드-라인/모아레 아티팩트를 불량 픽셀 보정과 호환 가능한 방식으로 억제.

CPU 기반 알고리즘만 사용하고 이 단계에서는 GUI가 없는 DLL (또는 공유 라이브러리) API 제공.

후속 R&D 계획에서 이러한 방법을 CPU + GPU로 확장할 예정입니다. 향후 방향은 여기서 설명하지만 구현되지 않습니다.

2. 기술적 기초
2.1 불량 픽셀/클러스터 보정
Lee et al.은 플랫-패널 방사선 촬영에서 3×3 및 5×5 클러스터 크기의 픽셀 결함 보정을 위한 ANN, CNN, concat-CNN, GAN 방법을 입증했습니다.

3×3 클러스터의 경우:

주변 픽셀을 사용한 단층 ANN (숨겨진 계층 없음)은 전통적인 템플릿 매칭 보정보다 훨씬 낮은 MSE를 달성했습니다.

5×5 클러스터의 경우:

ANN과 concat-CNN 모두 잘 작동했습니다. concat-CNN이 최고였지만 더 복잡했고, ANN은 여전히 훨씬 낮은 계산 오버헤드로 고전적 방법을 능가했습니다.

FixPix는 일반 이미징에서 불량 픽셀을 위한 경량 MLP 기반 재구성 및 세분화를 도입했습니다. 결과는 전체 결함률이 적으면 작은 MLP가 손상된 영역을 효과적으로 재구성할 수 있음을 확인했습니다.

2.2 라인 결함
CN104463831A는 X-ray FPD 불량 라인 수리를 위한 소프트웨어 전용 방법을 설명합니다:

이상 정도 diffVal은 결함 라인 픽셀과 인접한 무결 라인 사이의 차이에서 계산됩니다.

두 임계값 (T1, T2)이 적용할 보정 전략을 결정합니다:

T2 이상: 라인 값 무시, 이웃에서 보간.

T1과 T2 사이: 에지 감지 + 곡선 피팅 기반 보간.

2.3 그리드/모아레 아티팩트
정적 및 격자 그리드 아티팩트는 다음을 사용하여 억제할 수 있습니다:

DWT 기반 방법 (그리드라인 에너지가 특정 웨이블릿 부분대역을 지배하고 대역 저지 필터를 통해 감쇠).

DCT 기반 동적 분할 (이미지의 블록 또는 세그먼트가 변환되고 그리드 관련 주파수가 선택적으로 억제).

공간 도메인에서 그리드 구성 요소를 모델링하고 회귀하는 GRD (그리드 회귀/복조) 방법.

연구에 따르면 그리드 주파수가 해부학적 밴드에 앨리어싱될 때 (샘플링 불일치로 인해) 완벽한 복구는 불가능하며, 실용적 알고리즘은 진단 품질 저하 최소화와 함께 강력한 감쇠를 목표로 합니다.

이러한 사실은 실행 가능성을 기초하고 알고리즘 선택을 안내합니다.

3. 결함 검출 알고리즘
3.1 목표
검출 알고리즘은 다음을 수행해야 합니다:

식별:

단일 불량 픽셀.

클러스터 결함 (최소 5×5까지).

1~5 픽셀의 연속 폭을 가진 라인 결함.

그리드/모아레 아티팩트 및 그 심각도.

환자 안전과 패널 수명 간의 트레이드오프를 나타내는 Min/Normal/Max 결정 프로필 지원.

3.2 검출 파이프라인
Step 0: 캘리브레이션 기반 정적 결함 매핑

입력: 여러 어두운 필드 (X-ray 없음) 및 평탄 필드 (균일한 X-ray) 이미지.

계산:

오프셋 맵: 평균 어두운 이미지.

게인 맵: 평균 평탄 이미지에서 오프셋 차감, 정규화.

정적 픽셀 결함 검출:

오프셋 또는 게인이 보정 범위 밖인 픽셀은 정적 불량 픽셀이 됩니다.

저장:

정적 결함 맵 (단일 픽셀, 클러스터, 라인 후보).

Step 1: 이미지별 잔차 분석

각 임상 이미지에 대해:

로컬 중앙값 또는 평균 (예: 5×5 중앙값 필터)을 차감하여 잔차 맵 획득.

k·σ (로컬) 이상의 잔차 크기를 가진 픽셀은 동적 결함 후보가 됩니다.

Step 2: 연결된 구성 요소 및 형태 분석

정적 및 동적 후보를 이진 맵에 결합합니다.

연결된 구성 요소 레이블:

단일 픽셀 또는 매우 작은 구성 요소 → 고립 결함.

경계 상자가 3×3에 맞는 구성 요소 → 3×3 클러스터.

경계 상자가 5×5에는 맞지만 3×3에는 맞지 않는 구성 요소 → 5×5 클러스터.

행 또는 열을 따라 길쭉한 구성 요소 → 라인 결함 후보.

라인 후보의 경우:

각 행/열에 대해:

연속 결함 픽셀 실행 식별.

폭 (라인 방향에 수직) 및 길이 계산.

폭이 1~5 픽셀 사이인 세그먼트만 유지.

Step 3: 그리드/모아레 검출

결함 마스크로 전처리:

스파이크를 피하기 위해 초기 불량 픽셀 보정 (또는 평활) 사용.

그리드 분석 전에 결함 픽셀을 로컬 평균으로 대체.

주파수/다중 스케일 분석:

옵션 A (기준선): 2D DWT:

이미지에 대한 다중 레벨 DWT 수행.

그리드라인 에너지가 배경에 비해 강한 부분대역 식별 (예: 수직 그리드 → 수평 고주파 부분대역).

옵션 B (고급): 블록/세그먼트 DCT:

블록 또는 동적 세그먼트로 분할, 2D DCT 적용.

예상 그리드 주파수에서 강한 계수 검색.

모아레 심각도 지수 (MSI):

DWT/DCT 계수에서 그리드 관련 에너지와 총 이미지 에너지의 비율 계산.

MSI를 사용하여 심각도 클래스 결정 (예: 낮음, 중간, 높음).

3.3 Min/Normal/Max 프로필 (검출)
각 모드는 임계값 및 정책에 대한 매개변수 집합입니다:

Min (환자 중심):

잔차 기반 픽셀 검출 및 클러스터 크기에 대한 낮은 임계값 → 더 많은 결함 플래그됨.

라인 결함 분류 시 diffVal에 대한 낮은 임계값.

그리드 검출을 위한 낮은 MSI 임계값, 그리드 아티팩트가 남아있는 것에 대해 보수적.

Normal:

팬텀 및 회고 데이터를 사용하여 경험적으로 조정된 임계값으로 감도/특이성 균형 조정.

Max (패널 수명 중심):

더 높은 임계값: 명확히 해로운 결함만 분류.

정적 맵은 결함이 여러 이미지에 지속될 경우에만 업데이트.

그리드 억제가 트리거되기 전에 더 높은 MSI 임계값.

DLL은 이러한 구성을 선택하기 위해 DetectionMode를 노출합니다.

4. 결함 보정 알고리즘
4.1 목표
검출된 3×3 및 5×5 클러스터 결함 내의 모든 인접 불량 픽셀을 보정합니다.

1~5 픽셀 폭의 라인 결함을 다음으로 보정합니다:

유형 1, 3, 5, 우선순위 및 완성도 1 > 3 > 5.

유형 1과 3은 필수.

불량 픽셀 보정을 방해하지 않으면서 모아레/그리드 아티팩트를 억제합니다.

4.2 클러스터 보정 (3×3, 5×5)
4.2.1 3×3 클러스터 (유형 1 – 최고 우선순위)
Lee et al. 영감:

각 3×3 결함 영역에 대해:

결함을 중심으로 한 7×7 근처 영역 추출.

3×3 결함을 제외한 모든 7×7 픽셀을 취하여 입력 벡터 형성 (40개 입력).

사전 학습된 단층 ANN 사용:

입력: 40차원 벡터.

출력: 3×3 결함 블록의 9개 값.

아키텍처: 단순 y = W x + b, 선택적으로 매우 작은 숨겨진 계층.

3×3 블록의 모든 9개 픽셀을 ANN 출력으로 대체하고 유효 범위로 클리핑.

이 방식은 CPU 실행 가능하며 3×3 클러스터의 고전적 템플릿 매칭을 능가하는 것으로 실험적으로 입증되었습니다.

4.2.2 5×5 클러스터 (유형 3 – 필수, 3×3보다 낮은 우선순위)
Lee et al. 기반:

각 5×5 결함 영역에 대해:

결함을 중심으로 한 9×9 근처 영역 추출.

입력 벡터 형성: 중앙 5×5 블록을 제외한 9×9 픽셀 (56개 입력).

ANN 사용:

입력: 56차원 벡터.

출력: 5×5 결함 블록의 25개 값.

아키텍처: 작음 (예: 64개 단위의 한 개 숨겨진 계층) CPU에서 빠르게 유지.

5×5 블록의 모든 25개 픽셀을 ANN 출력으로 대체.

선택적으로 오프라인/고품질 모드:

템플릿 매칭 상관 (TMC) 적용:

ANN으로 결함을 대략 채우기.

최적 일치 패치를 더 큰 근처 영역 (예: 27×27)에서 검색하고 해당 패치 중심을 사용하여 5×5 값 개선.

이는 5×5 결함의 모든 인접 픽셀이 보정되는 요구사항을 충족합니다.

4.3 라인 결함 보정 (폭 1~5)
CN104463831A 기반, 폭 1~5로 일반화.

Step 1: 라인당 이상 정도 (diffVal)

각 검출된 라인 세그먼트 (행 또는 열, 폭 1~5)에 대해:

각 결함 픽셀 값을 인접한 무결 라인의 평균과 비교.

최대 회색 레벨 및 픽셀 수로 정규화하여 diffVal 획득.

Step 2: 보정 유형 결정

임계값 T1과 T2 사용 (모드 의존):

유형 1 (심각, 필수): diffVal > T2

원래 라인 픽셀을 무효로 취급.

즉시 인접한 무결 이웃 라인에서 보간:

수평 라인의 경우: 위/아래 행 사용.

수직 라인의 경우: 좌/우 열 사용.

라인 방향을 따라 1D 평활 적용 (예: 작은 Gaussian).

유형 3 (중간, 필수): T1 < diffVal ≤ T2

에지 인식 보정:

라인에 평행한 에지를 감지하기 위해 Sobel 필터 적용.

라인을 따라 에지 위치에 대한 이차 곡선 피팅 수행.

라인 픽셀 보간 조합:

이웃 기반 보간.

피팅된 에지 곡선에서 파생된 값.

유형 5 (경미, 선택): diffVal ≤ T1

Min/Normal/Max에 따라:

Min: 미세한 아티팩트를 피하기 위해 여전히 가벼운 보간 사용 가능.

Normal: 최소한의 평활 적용 또는 변경 없이 유지.

Max: 일반적으로 더 큰 문제의 일부가 아니면 변경 없이 유지.

폭 >1 (최대 5)의 경우, 결함 밴드의 각 열/행을 에지 일관성을 유지하면서 조정된 방식으로 처리합니다.

4.4 그리드/모아레 억제
픽셀/라인 결함 보정 후:

Step 1: 그리드 분석을 위한 전처리된 이미지

그리드 억제의 경우, 불량 픽셀/라인이 보정된 이미지에서 시작합니다.

선택적으로 보정된 영역을 약간 평활화하거나 변환 전에 로컬 평균으로 대체하여 잔차 스파이크를 피합니다.

Step 2: 기준선 방법 – DWT 기반 억제

Tang et al.과 같이 다중 레벨 2D DWT 사용:

부분대역으로 분해.

그리드 에너지가 지배하는 부분대역 식별.

해당 부분대역에 적응형 Gaussian 대역 저지 필터 적용.

역 DWT로 이미지 재구성.

Step 3: 고급 방법 – DCT 기반 동적 분할

이미지를 블록 또는 동적 세그먼트로 분할합니다.

세그먼트당 2D DCT 적용.

예상 그리드 주파수 (및 고조파) 근처의 계수 감쇠.

Step 4: 선택적 GRD/복조 (실험)

높은 MSI (심각한 모아레)의 경우, 선택적으로 그리드 회귀/복조를 적용하여 공간 도메인에서 잔차 그리드 구성 요소를 모델링하고 제거합니다.

4.5 Min/Normal/Max 프로필 (보정)
Min:

항상 ANN을 통해 3×3 및 5×5 클러스터 보정.

항상 유형 1 및 3 라인 보정 적용, 가능한 곳에 유형 5 적용.

그리드 억제를 위해 더 강력한 DWT/DCT 필터 (더 큰 감쇠) 사용.

Normal:

동일 필수 보정 (3×3, 5×5, 유형 1/3 라인).

유형 5 보정 및 필터 강도를 균형으로 조정.

Max:

명확히 심각한 결함의 필수 항목 변경 없음 (3×3, 5×5, 유형 1 라인).

유형 3 및 유형 5 보정은 diffVal/MSI가 명확히 위험을 나타낼 경우에만 적용.

영향 및 처리 빈도를 최소화하기 위한 약한 그리드 억제.

5. 소프트웨어 아키텍처 (DLL) 및 구현 계획
5.1 API 개요
DLL은 C/C++ 함수 (GUI 없음)를 노출합니다:

초기화:

InitLibrary(const Config* cfg);

LoadCalibration(const OffsetMap* offset, const GainMap* gain);

SetMode(DetectionMode mode); // MIN, NORMAL, MAX

핵심 처리:

DetectDefects(const Image* in, DefectMask* mask, DefectStats* stats);

CorrectDefectsAndSuppressGrid(const Image* in, const DefectMask* mask, Image* out);

또는 결합:

ProcessImage(const Image* in, Image* out); // 내부 파이프라인

5.2 구현 고려사항
언어: C/C++.

최적화:

ANN 추론 및 필터링을 위해 SIMD (예: AVX) 사용 (이득이 있을 경우).

타일 또는 블록을 통한 파이프라인 멀티스레드화.

의존성:

ANN 행렬 연산을 위한 경량 수학 루틴 또는 BLAS.

그리드 억제를 위한 FFT/DWT/DCT 라이브러리 (예: FFTW 또는 맞춤).

6. 향후 CPU+GPU 단계 (깊은 연구 트랙)
후속 R&D 계획에서 다음을 탐색할 것입니다:

U-Net 유사 모델을 사용한 불량 픽셀 및 그리드 아티팩트에 대한 GPU 가속 세분화.

3×3 및 5×5 클러스터 보정을 위한 Concat-CNN 및 GAN 기반 보정 (ANN 이상으로 MSE 개선 가능).

조인트 링/그리드 및 결함 보정을 위한 암시적 신경 표현 및 트랜스포머 기반 재구성.

이들은 향후 단계로 명시적으로 연기되었습니다. 현재 계획은 모든 알고리즘이 CPU 실행 가능하고 공개 방법에 기초함을 보장합니다.

7. Reference

Bad pixel / cluster / line defect 관련
Using deep learning for pixel-defect corrections in flat-panel radiography imaging

내용: DR용 FPD에서 3×3, 5×5 pixel defect를 ANN, CNN, concat-CNN, GAN으로 보정하는 방법 및 성능 비교.

Pixel-defect corrections for radiography detectors based on deep learning (SPIE)

내용: radiography detector의 pixel-defect correction을 위한 딥러닝 접근 개요.
​

FixPix: Fixing Bad Pixels using Deep Learning

내용: 일반 이미지 센서에서 segmentation + lightweight MLP/ViT AE로 bad pixel을 검출/보정하는 방법.

방사선 디텍터에서 CNN 기법을 사용한 픽셀 결함 보정 (국내 논문)

내용: X-ray 디텍터에서 CNN 기반 화소 결함 보정 알고리즘 제안.
​

CN104463831A – Method for repairing X-ray flat panel detector image bad line

내용: X-ray FPD 이미지의 bad line을 diffVal, 두 개의 threshold(T1/T2), interpolation 및 edge-aware smoothing으로 소프트웨어로 보정하는 특허.
​

How to repair the broken flat panel detector

내용: FPD 필드 고장, defect 대응(교체·수리)에 대한 실무적 설명.
​

Grid / Moiré / aliasing 관련
A new stationary gridline artifact suppression method based on the 2D discrete wavelet transform

내용: DWT 기반으로 gridline artifact를 검출·감쇠하는 방법.
​

A Study of Grid Artifacts Formation and Elimination in Computed Radiographic Images

내용: CR/DR에서 grid artifact 발생 메커니즘과 제거 방법, aliasing 한계 분석.
​

A Dynamically Segmented DCT Technique for Grid Artifact Suppression in X-ray Images (및 한국어 논문 버전)

내용: X-ray 영상에서 동적 분할 + DCT 기반 grid artifact 제거 기법.

A novel grid regression demodulation method for radiographic grid artifact correction

내용: spatial domain에서 grid 성분을 회귀 모델로 추정하고 제거하는 GRD 방식.
​

A software-based method for eliminating grid artifacts of a high resolution image detector

내용: 고해상도 이미지 디텍터의 grid artifact를 소프트웨어로 제거하는 방법.
​

Patch Based Grid Artifact Suppressing in Digital Mammography

내용: 패치 기반 grid artifact suppression.

X-ray 영상에서 그리드 아티팩트 개선을 위한 동적 분할 기반 DCT 기법

내용: 국내 DCT 기반 grid artifact 개선 방식 상세.

기타 참고 (품질/딥러닝 전반)
Deep Learning Neural Network Performance on NDT Digital X-ray Radiography Images

내용: NDT X-ray 이미지에서 딥러닝 성능에 대한 실험 연구 (이미지 품질 파라미터 영향).
​

Deep Learning Image Reconstruction for CT: Technical Principles and Clinical Prospects

내용: CT에서 딥러닝 기반 재구성·artifact 보정의 기술 및 임상적 관점.
​

Quick guide on radiology image pre-processing for deep learning applications

내용: 방사선 영상에서 딥러닝 전처리에 필요한 일반적 artifact 처리 개요.
