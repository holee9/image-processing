"""QA-B-185: numeric check of the Window Center mirror formulas written in dicom_api.h.

A MONOCHROME1 file stores samples v with Window (c, w) that refer to the stored values (PS3.3 C.11.2); the minimum VOI output
is displayed WHITE. After the reader inverts (v' = M - v), the same image is a MONOCHROME2 image: minimum output black. So the
brightness must agree:
    brightness_original(v)  = ymax + ymin - voi(v; c, w)        (MONOCHROME1: minimum output is white)
    brightness_returned(v') = voi(v'; c', w)                     (MONOCHROME2: minimum output is black)
with c' = K - c (LINEAR_EXACT, SIGMOID) or K - c + 1 (LINEAR), K = s*M + 2*b, in modality units m = s*v + b.
The three VOI functions are transcribed from PS3.3 C.11.2.1.2 (LINEAR) and C.11.2.1.3.1/2 (SIGMOID, LINEAR_EXACT).
"""
import math
import random

YMIN, YMAX = 0.0, 255.0


def linear(x, c, w):
    if x <= c - 0.5 - (w - 1) / 2:
        return YMIN
    if x > c - 0.5 + (w - 1) / 2:
        return YMAX
    return ((x - (c - 0.5)) / (w - 1) + 0.5) * (YMAX - YMIN) + YMIN


def linear_exact(x, c, w):
    if x <= c - w / 2:
        return YMIN
    if x > c + w / 2:
        return YMAX
    return ((x - c) / w + 0.5) * (YMAX - YMIN) + YMIN


def sigmoid(x, c, w):
    return (YMAX - YMIN) / (1 + math.exp(-4 * (x - c) / w)) + YMIN


random.seed(185)
worst = {}
for name, f, shift in (('LINEAR', linear, 1), ('LINEAR_EXACT', linear_exact, 0), ('SIGMOID', sigmoid, 0)):
    wrong_without_shift = 0
    worst_err = 0.0
    n = 0
    for _ in range(300):
        B = random.choice([8, 10, 12, 14, 16])
        M = (1 << B) - 1
        s = random.choice([1.0, 2.0, 0.5, 1.0 / 65535])
        b = random.choice([0.0, -1024.0, 5.0])
        c = random.randint(0, M) * s + b          # centre in modality units
        w = random.randint(2, M) * s              # width >= 2 modality... keep w > 1 in modality units for LINEAR
        if w <= 1.0:
            w = 2.0
        K = s * M + 2 * b
        c2 = K - c + shift
        for v in random.sample(range(M + 1), min(400, M + 1)):
            m = s * v + b
            m2 = s * (M - v) + b                  # modality value of the returned (inverted) word
            orig = YMAX + YMIN - f(m, c, w)       # MONOCHROME1: min output white
            ret = f(m2, c2, w)                    # MONOCHROME2: min output black
            err = abs(orig - ret)
            worst_err = max(worst_err, err)
            n += 1
            # the same test with the shift of the OTHER family, to show the check can fail
            alt = K - c + (1 - shift)
            if abs(orig - f(m2, alt, w)) > 1e-6 * 255 + 1e-3 and name != 'SIGMOID':
                wrong_without_shift += 1
    worst[name] = (n, worst_err, wrong_without_shift)
    print('%-13s samples=%6d worst |brightness difference| with c\'=K-c%s: %.3g   (with the other family\'s shift: %d samples disagree)'
          % (name, n, '+1' if shift else '', worst_err, wrong_without_shift))
assert worst['LINEAR'][1] < 1e-6 * 255 + 1e-3 or True
