"""출력 영상 ↔ Target 영상 유사도(loss) 및 지표.

비교는 [0,1] 정규화 후 ref-map(조사야) 내부 침식(eroded) 마스크에서 수행한다.
- 조사야 경계 침식: 11×11 SSIM 가우시안 창 및 다중 스케일 Sobel 커널이 조사야 밖(셔터 영역)의
  급격한 경계값을 끌어와 지표를 오염시키는 현상을 방지한다.
- SSIM 수치 안정성: float32 누적 오차로 인한 음수 분산(saa, sbb < 0) 클램핑 및 분모 하한 보장.
- 다중 스케일 Edge 손실: 1×, 1/2×, 1/4× (가중치 0.5, 0.3, 0.2) Sobel 경사 크기를 함께 비교해
  고주파 선예도(DETAIL)뿐 아니라 중·저주파 구조 대역(STRUCT, IBDC, NECR) 변화도 포착한다.

loss = w_pix*L1*10 + w_ssim*(1-SSIM) + w_edge*mean|Δ|∇||_ms*10 + w_hist*mean|Δquantile|*10
(*10 은 항목 간 크기 균형용 상수)
"""
from dataclasses import dataclass

import cv2
import numpy as np

_QS = np.array([1, 2, 5, 10, 20, 30, 40, 50, 60, 70, 80, 90, 95, 98, 99], dtype=np.float64)
_EDGE_SCALES = ((1.0, 0.5), (0.5, 0.3), (0.25, 0.2))


@dataclass
class LossWeights:
    pix: float = 1.0
    ssim: float = 1.0
    edge: float = 1.0
    hist: float = 1.0


def _ssim_map(a, b, c1=0.01 ** 2, c2=0.03 ** 2, mu_b=None, mu_b2=None, sbb=None):
    k = (11, 11)
    g = lambda x: cv2.GaussianBlur(x, k, 1.5)
    mu_a = g(a)
    if mu_b is None:
        mu_b = g(b)
    mu_a2 = mu_a * mu_a
    if mu_b2 is None:
        mu_b2 = mu_b * mu_b
    # float32 가우시안 필터링의 부동소수점 반올림 오차로 분산이 미세 음수가 되는 현상 방지
    saa = np.maximum(g(a * a) - mu_a2, 0.0)
    if sbb is None:
        sbb = np.maximum(g(b * b) - mu_b2, 0.0)
    sab = g(a * b) - mu_a * mu_b
    num = (2.0 * mu_a * mu_b + c1) * (2.0 * sab + c2)
    den = (mu_a2 + mu_b2 + c1) * (saa + sbb + c2)
    return num / np.maximum(den, 1e-12)


def _grad_mag(x):
    gx = cv2.Sobel(x, cv2.CV_32F, 1, 0, ksize=3)
    gy = cv2.Sobel(x, cv2.CV_32F, 0, 1, ksize=3)
    return cv2.magnitude(gx, gy)


class LossEvaluator:
    def __init__(self, target: np.ndarray, pix_max: int, colli_map_ref: np.ndarray,
                 eval_scale: float = 1.0, weights: LossWeights = None):
        self.pix_max = float(pix_max)
        self.w = weights or LossWeights()
        self.scale = float(eval_scale)
        h, w = target.shape
        self.size = (max(8, int(round(w * self.scale))), max(8, int(round(h * self.scale))))
        self.target = self._prep(target)
        m_u8 = cv2.resize((colli_map_ref > 0).astype(np.uint8), self.size,
                          interpolation=cv2.INTER_NEAREST)
        # 11×11 가우시안 창(반경 5) 및 다중 스케일 Sobel 경계 오염 방지를 위해 조사야 마스크 침식
        erode_r = max(1, int(round(5 * self.scale)))
        kern = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (2 * erode_r + 1, 2 * erode_r + 1))
        m_eroded = cv2.erode(m_u8, kern).astype(bool)
        if m_eroded.sum() >= 100:
            m = m_eroded
        elif m_u8.sum() >= 100:
            m = m_u8.astype(bool)
        else:
            m = np.ones(self.size[::-1], dtype=bool)
        self.mask = m

        # Target 영상의 SSIM 가우시안 통계(mu_b, mu_b2, sbb) 사전 계산 (매 평가마다 GaussianBlur 2회 절감)
        self._t_mu = cv2.GaussianBlur(self.target, (11, 11), 1.5)
        self._t_mu2 = self._t_mu * self._t_mu
        self._t_sbb = np.maximum(
            cv2.GaussianBlur(self.target * self.target, (11, 11), 1.5) - self._t_mu2, 0.0)

        # 다중 스케일(1×, 1/2×, 1/4×) Sobel 경사 기준값 및 마스크 사전 계산
        self._edge_levels = []
        base_w, base_h = self.size
        m_float = m.astype(np.uint8)
        for sf, sw in _EDGE_SCALES:
            if sf == 1.0:
                t_s = self.target
                m_s = m
                sz_s = self.size
            else:
                sz_s = (max(8, int(round(base_w * sf))), max(8, int(round(base_h * sf))))
                t_s = cv2.resize(self.target, sz_s, interpolation=cv2.INTER_AREA)
                m_s = cv2.resize(m_float, sz_s, interpolation=cv2.INTER_NEAREST).astype(bool)
                if m_s.sum() < 16:
                    m_s = np.ones(sz_s[::-1], dtype=bool)
            self._edge_levels.append((sf, sw, sz_s, m_s, _grad_mag(t_s)))

        self.t_q = np.percentile(self.target[m], _QS)

    def _prep(self, img) -> np.ndarray:
        x = np.asarray(img, dtype=np.float32) / self.pix_max
        if self.scale != 1.0:
            x = cv2.resize(x, self.size, interpolation=cv2.INTER_AREA)
        return np.ascontiguousarray(x)

    def evaluate(self, out: np.ndarray):
        """returns (loss, detail dict)"""
        o = self._prep(out)
        m, t = self.mask, self.target
        o_m = o[m]
        diff_m = o_m - t[m]
        l1 = float(np.abs(diff_m).mean())
        ssim = float(_ssim_map(
            o, t, mu_b=self._t_mu, mu_b2=self._t_mu2, sbb=self._t_sbb)[m].mean())
        edge = 0.0
        for sf, sw, sz_s, m_s, t_grad_s in self._edge_levels:
            o_s = o if sf == 1.0 else cv2.resize(o, sz_s, interpolation=cv2.INTER_AREA)
            edge += sw * float(np.abs(_grad_mag(o_s) - t_grad_s)[m_s].mean())
        hist = float(np.abs(np.percentile(o_m, _QS) - self.t_q).mean())
        w = self.w
        loss = (w.pix * l1 * 10 + w.ssim * (1 - ssim) + w.edge * edge * 10 + w.hist * hist * 10)
        mse = float((diff_m * diff_m).mean())
        psnr = 10.0 * np.log10(1.0 / max(mse, 1e-12)) if mse > 1e-12 else 99.0
        return loss, {"loss": loss, "mae": l1 * self.pix_max, "psnr": psnr, "ssim": ssim,
                      "l1": l1, "edge": edge, "hist": hist}
