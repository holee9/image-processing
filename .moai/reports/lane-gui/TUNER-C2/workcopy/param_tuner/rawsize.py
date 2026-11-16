"""raw 영상 크기 추정.

raw 파일에는 헤더가 없어서 파일 크기(바이트)만 알 수 있다. 14bit(int16)와 16bit(uint16) 모두
화소당 2바이트이므로  파일 크기 ÷ 2 = 가로 × 세로  를 만족하는 (가로, 세로) 후보를 구한다.

- 알려진 크기(KNOWN_SIZES + 사용자가 이전에 사용한 크기)와 일치하면 우선한다.
- 알려진 크기가 아니면 검출기 영상으로 자연스러운 후보만 남긴다: 가로/세로 ≥ 256, 둘 다 짝수,
  긴 변 / 짧은 변 ≤ 1.8.
"""
import math

KNOWN_SIZES = [(3040, 2466), (3040, 3036)]     # (가로, 세로) — 확인된 검출기 영상 크기

_TYPICAL_RATIOS = (1.0, 1.2328, 1.25, 4 / 3, 1.2)     # 후보 정렬용 (정사각/검출기 비율)
MAX_CANDIDATES = 12


def candidate_sizes(nbytes: int, known=(), bytes_per_pixel: int = 2):
    """가능한 (가로, 세로, 알려진_크기_여부) 목록. 알려진 크기가 먼저, 이후 전형적인 비율에 가까운 순."""
    if nbytes <= 0 or nbytes % bytes_per_pixel:
        return []
    n = nbytes // bytes_per_pixel
    known = {tuple(k) for k in known}
    out = []
    for w in range(256, 8193):
        if n % w:
            continue
        h = n // w
        if not 256 <= h <= 8192:
            continue
        is_known = (w, h) in known
        ratio = max(w, h) / min(w, h)
        if not is_known and (ratio > 1.8 or w % 2 or h % 2):
            continue
        out.append((w, h, is_known))

    def key(c):
        w, h, k = c
        ratio = max(w, h) / min(w, h)
        dist = min(abs(math.log(ratio / r)) for r in _TYPICAL_RATIOS)
        return (not k, w < h, dist)        # 알려진 크기 → 가로형 우선 → 전형적 비율에 가까운 순
    out.sort(key=key)
    return out[:MAX_CANDIDATES]
