"""run_pipeline.py 의 main() 로직을 함수형으로 재구성한 실행기.

- 1~13단계(검출기 정규화, 조사야/피사체 맵, log 변환, ROI 마스크)는
  파라미터와 무관하므로 Source 당 1회만 계산(prepare)하고 캐시한다.
- 14~17단계(slope 보정, Multi-Scale, 노이즈 감소, Manual LUT)는 파라미터 의존.
  중간 결과를 다계층 LRU 캐시하여 변경된 하위 단계만 재계산한다.
    slope+MSE 캐시 키 : PostParam 중 [23..26] 제외 전부
    MSE 내부 재사용   : enhance_multiscale_detail() 을 단계별로 나눠 같은 함수를 같은 인자로 호출하되 중간 결과를 재사용
                        - object_map_os(업샘플 ROI 마스크) : Source 당 1회(prepare)
                        - slope 보정 + 4대역 피라미드 분해(img_bl3, img_dl_raw) : (body_part, slope, mst_field, rot, e_band_ind) 키별 캐시
                        - Base 계조(ROI 통계 → Center Sigmoid → Tone → Latitude → Contrast LUT) : 계조 파라미터 단계별 캐시
                        - 과대비 억제 1(분포 기반, reduce_excess_contrast) : (피라미드 키 + Decay/DCS2) 키별 캐시
                        - 과대비 억제 2(베이스 기반, reduce_excess_contrast_by_base) : (Base 키 + ECR1 키 + IBDC) 키별 캐시
                        (결과는 enhance_multiscale_detail() 과 비트 단위로 동일해야 하며 verify_runner.py 로 검증)
    NR 캐시 키        : MSE 키 + PostParam[23..26]
    NR 내부 재사용    : reduce_noise() 를 단계별로 나눠 같은 함수를 같은 인자로 호출하되 중간 결과를 재사용
                        - 노이즈 공분산맵        : 입력 원본(DRN)만으로 계산 → Source 당 1회
                        - 디노이즈 결과/노이즈/백색화 노이즈 : MSE 출력에만 의존 (NR 강도·백색화 값과 무관)
                          → MSE 키별 캐시. NR 강도/백색화만 바뀌면 블렌딩만 다시 계산
                        (결과는 reduce_noise() 와 비트 단위로 동일해야 하며 verify_runner.py 로 검증)
    LUT               : 항상 재실행 (가장 가벼움)
- run_pipeline.py 는 수정하지 않는다 (로직만 동일하게 복제).
"""
from collections import OrderedDict
from dataclasses import dataclass, field
from typing import Callable, Optional

import numpy as np

from pe_post.common import (CHEST_ORIENTATION_TYPES, PHYSICAL_TEST, PIXEL_DTYPE, WHOLE_BODY_AP, # type: ignore
                            WHOLE_BODY_LAT, LONG_BONE_AP, PIX_MAX,
                            to_analysis_domain)
from pe_post.params import build_post_processing_params # type: ignore
from pe_post.context import PostProcessingContext # type: ignore
from pe_post.detector_normalization import normalize_detector_response # type: ignore
from pe_post.collimator_map import build_collimator_map # type: ignore
from pe_post.object_map import build_object_map # type: ignore
from pe_post.log_transform import apply_log_transform, LOG_MIN_DIFF_TONE_FULL # type: ignore
from pe_post.utils import image_mean, img_resize # type: ignore
from pe_post.object_map_refine import refine_object_map_by_edge_rate, compensate_object_map # type: ignore
from pe_post.roi_maps import refine_collimator_map, build_rough_roi_map, build_roi_masks # type: ignore
from pe_post.slope_correction import correct_intensity_slope # type: ignore
from pe_post.multiscale_enhance import ( # type: ignore
    apply_center_sigmoid,
    apply_tone_representation,
    compensate_latitude,
    decompose_pyramid,
    enhance_multiscale_detail,
    evaluate_roi_statistics,
    reduce_excess_contrast,
    reduce_excess_contrast_by_base,
    reduce_excess_contrast_by_detail,
)
from pe_post.noise_reduction import (build_multiscale_nlm_plan, denoise_multiscale, # type: ignore
                                     denoise_multiscale_with_plan,
                                     estimate_noise_covariance_map,
                                     reduce_noise, whiten_noise)
from pe_post.bezier_lut import DEFAULT_OUTPUT_BIT_DEPTH, fit_bezier_lut_4points # type: ignore

from param_tuner.param_space import Params # type: ignore

_NR_IDX = (23, 24, 25, 26)
_T_SIGMA = [-0.2, -1.5, -1.0, -0.5, 0.0, 0.5, 1.0, 1.5]
_F_PIXEL_SPACE = 0.143


@dataclass
class RunSettings:
    width: int = 3040
    height: int = 2466
    object_type: int = 1107          # C++ TObjectType (Chest PA)
    shutter_points: np.ndarray = field(default_factory=lambda: np.array(
        [[10, 0], [10, 2466], [3026, 2466], [3026, 0]], dtype=np.int32))
    collimator_edge_radius: int = 15


class _LRU(OrderedDict):
    def __init__(self, cap):
        super().__init__()
        self.cap = cap

    def put(self, k, v):
        self[k] = v
        self.move_to_end(k)
        while len(self) > self.cap:
            self.popitem(last=False)

    def fetch(self, k):
        if k in self:
            self.move_to_end(k)
            return self[k]
        return None


class PipelineRunner:
    def __init__(self, src_img: np.ndarray, settings: RunSettings,
                 log: Optional[Callable[[str], None]] = None):
        self.src = src_img
        self.s = settings
        self.log = log or (lambda m: None)
        self.ready = False
        self.object_map_os = None
        self._mse_cache = _LRU(3)
        self._pyr_cache = _LRU(2)        # (body, slope, mst, rot, e_band_ind) -> 피라미드 분해 결과
        self._roi_cache = _LRU(2)        # ROI 통계(evaluate_roi_statistics) 결과
        self._sig_cache = _LRU(3)        # ROI 통계 + Center Sigmoid 결과 (int16)
        self._tone_cache = _LRU(3)       # Tone Representation 결과 (int16)
        self._lat_cache = _LRU(3)        # Latitude 보상 결과 (int16)
        self._base_cache = _LRU(3)       # Latitude + Contrast LUT 결과 (px_img_out, f_density_gain)
        self._ecr1_cache = _LRU(2)       # 분포 기반 과대비 억제(reduce_excess_contrast) 4대역 결과
        self._ecr2_cache = _LRU(2)       # 베이스 기반 과대비 억제(reduce_excess_contrast_by_base) 결과
        self._nr_cache = _LRU(3)
        self._nr_parts = _LRU(2)         # MSE 키 -> {"out_f", "noise", "nw"} (큰 float32 배열이라 2개만 유지)
        self._lut_cache = _LRU(8)        # (tuple(p.lut), output_bit_depth) -> 1D LUT (PIXEL_DTYPE)
        self._cov_map = None             # 노이즈 공분산맵 (입력 원본에만 의존 → 1회 계산)
        self._nlm_plan = None            # 다중스케일 NLM 가중치 플랜 (입력 원본에만 의존 → 1회 계산)
        self._iw_f = None
        self._frozen_nr_ref = None       # MSE 그룹 탐색 중 고정할 기준 NR 잔차 (ref_mse_key, parts)
        self.eval_count = 0

    # ------------------------------------------------------------------
    def prepare(self, p: Params):
        """1~13단계 (파라미터 무관) 계산 후 캐시."""
        s = self.s
        param = build_post_processing_params(s.width, s.height,
                                             s.object_type, p.post, p.lut)
        ctx = PostProcessingContext()
        ctx.init(param)
        ctx.create()
        ctx.input_image = self.src
        ctx.auto_shutter_points = np.asarray(s.shutter_points, dtype=np.int32)
        ctx.select_roi_points()
        ctx.allocate_buffers()
        body = ctx.body_part
        rw, rh = ctx.ref_map_width, ctx.ref_map_height

        img = normalize_detector_response(ctx.input_image)                  # 1
        img14 = to_analysis_domain(img)
        colli_map = build_collimator_map(ctx.roi_points, rw, rh,
                                         s.collimator_edge_radius, body)     # 3
        colli_org = colli_map.copy()
        img_sub = img_resize(img14, rw, rh)                                  # 4
        ob_map, n_de = build_object_map(img_sub, body)
        log_tone = apply_log_transform(img, LOG_MIN_DIFF_TONE_FULL)          # 6
        if image_mean(log_tone) <= 0:                                        # 7
            raise RuntimeError("전체 포화 입력: All-Zero 출력 (처리 불가)")

        if body in CHEST_ORIENTATION_TYPES:                                     # 9-1
            ob_map = refine_object_map_by_edge_rate(log_tone, ob_map, colli_map, img_sub)
        img_sub = img_resize(img14, rw, rh)                                  # 9-2
        if (body not in (WHOLE_BODY_AP, WHOLE_BODY_LAT, LONG_BONE_AP)
                and max(s.width, s.height) < 3200):
            ob_map = compensate_object_map(img14, ob_map, body)
        if body != PHYSICAL_TEST:                                            # 10
            colli_map = refine_collimator_map(colli_map, ob_map, img14, body)
        colli_map = build_rough_roi_map(img_sub, colli_map, n_de, body)      # 11
        roi_l, roi_s = build_roi_masks(colli_map, ob_map)                    # 13

        self.img, self.log_tone = img, log_tone
        self.colli_org, self.colli_map = colli_org, colli_map
        self.roi_l, self.roi_s = roi_l, roi_s
        self.object_map_os = img_resize(roi_l.astype(PIXEL_DTYPE), s.width, s.height)
        self.ref_size = (rw, rh)
        self._mse_cache.clear()
        self._pyr_cache.clear()
        self._roi_cache.clear()
        self._sig_cache.clear()
        self._tone_cache.clear()
        self._lat_cache.clear()
        self._base_cache.clear()
        self._ecr1_cache.clear()
        self._ecr2_cache.clear()
        self._nr_cache.clear()
        self._nr_parts.clear()
        self._lut_cache.clear()
        self._cov_map = None
        self._nlm_plan = None
        self._iw_f = None
        self._frozen_nr_ref = None
        self.ready = True
        self.log(f"[prepare] 1~13단계 완료 (refmap {rw}x{rh}, colli px={int(colli_map.sum())})")

    # ------------------------------------------------------------------
    def _make_ctx(self, p: Params) -> PostProcessingContext:
        s = self.s
        param = build_post_processing_params(s.width, s.height,
                                             s.object_type, p.post, p.lut)
        ctx = PostProcessingContext()
        ctx.init(param)
        ctx.create()
        ctx.rotation_angle = 0
        if ctx.body_part in CHEST_ORIENTATION_TYPES:                         # 12
            ctx.gradation_params.measurement_field_type = 2
        return ctx

    def _apply_manual_bezier_lut_cached(self, img: np.ndarray, cp, output_bit: int,
                                        lut_tuple: tuple) -> np.ndarray:
        """apply_manual_bezier_lut(img, cp, output_bit, i_max=PIX_MAX, i_min=0) 와
        비트 단위로 동일한 이미지 매핑을 수행하되, 1D LUT 를 캐싱하고 미사용 전/후 히스토그램 계산을 생략한다."""
        lut_key = (lut_tuple, output_bit)
        lut_1d = self._lut_cache.fetch(lut_key)
        if lut_1d is None:
            hist_max = int(2.0 * PIX_MAX)
            hist_min = -PIX_MAX
            d_max = int(PIX_MAX * cp.cp_y_end)
            d_min = max(0, int(PIX_MAX * cp.cp_y_start))
            s_max = PIX_MAX
            s_min = 0

            lut = fit_bezier_lut_4points(cp, d_max, d_min, hist_max, hist_min, s_max, s_min)
            if output_bit != DEFAULT_OUTPUT_BIT_DEPTH:
                shift = DEFAULT_OUTPUT_BIT_DEPTH - output_bit
                d_max >>= shift
                d_min >>= shift
                sl = slice(s_min - hist_min, s_max - hist_min + 1)
                lut[sl] = lut[sl] >> shift

            # 인덱스 0: data < 0 (-> d_min), 인덱스 1..PIX_MAX+1: 0..PIX_MAX, 인덱스 PIX_MAX+2: data > PIX_MAX (-> d_max)
            lut_1d = np.empty(PIX_MAX + 3, dtype=PIXEL_DTYPE)
            lut_1d[0] = d_min
            lut_1d[1:PIX_MAX + 2] = np.clip(
                lut[s_min - hist_min:s_max - hist_min + 1], d_min, d_max).astype(PIXEL_DTYPE)
            lut_1d[PIX_MAX + 2] = d_max
            self._lut_cache.put(lut_key, lut_1d)

        return lut_1d[np.clip(img.astype(np.int32), -1, PIX_MAX + 1) + 1]

    def begin_mse_group(self, ref_params: Params):
        """MSE 그룹(계조/톤, 주파수/선예도, 과대비 억제) 탐색 시작 시 호출.
        NR 이 활성화된 상태(post[24] == 0)라면 현재 최적 파라미터(ref_params)의
        정확한 NR 노이즈 잔차 성분을 확보하여 그룹 내 후보 탐색 중 고속 재사용한다."""
        if not self.ready:
            self.prepare(ref_params)
        ctx = self._make_ctx(ref_params)
        fq = ctx.frequency_params
        f_red = min(fq.noise_reduction_amount, 80.0)
        f_wht = min(max(fq.noise_whitening_amount, 0.0), 100.0)
        if fq.noise_reduction_type != 0 or (f_red == 0.0 and f_wht == 0.0):
            self._frozen_nr_ref = None
            return

        mse_key = tuple(v for i, v in enumerate(ref_params.post) if i not in _NR_IDX)
        mse_entry = self._mse_cache.fetch(mse_key)
        if mse_entry is None:
            px_mse, all_sat = self._enhance_multiscale_cached(ctx)
            mse_entry = (px_mse, all_sat)
            self._mse_cache.put(mse_key, mse_entry)
        else:
            px_mse, all_sat = mse_entry
        if all_sat:
            self._frozen_nr_ref = None
            return

        parts = self._ensure_nr_parts(mse_key, px_mse, f_red, f_wht)
        self._frozen_nr_ref = (mse_key, parts)

    def end_mse_group(self):
        """MSE 그룹 탐색 종료 시 고정된 NR 잔차 모드를 해제한다."""
        self._frozen_nr_ref = None

    def run(self, p: Params) -> np.ndarray:
        """14~17단계 실행 → 출력 영상 (PIXEL_DTYPE, 0~PIX_MAX)."""
        if not self.ready:
            self.prepare(p)
        self.eval_count += 1
        ctx = self._make_ctx(p)
        mse_key = tuple(v for i, v in enumerate(p.post) if i not in _NR_IDX)
        nr_key = mse_key + tuple(p.post[i] for i in _NR_IDX)

        px_nr = self._nr_cache.fetch(nr_key)
        if px_nr is None:
            mse_entry = self._mse_cache.fetch(mse_key)
            if mse_entry is None:
                px_mse, all_sat = self._enhance_multiscale_cached(ctx)        # 14~15
                mse_entry = (px_mse, all_sat)
                self._mse_cache.put(mse_key, mse_entry)
            else:
                px_mse, all_sat = mse_entry
                if all_sat:
                    ctx.frequency_params.noise_reduction_amount = 0
            fq = ctx.frequency_params
            use_frozen = (
                self._frozen_nr_ref is not None
                and fq.noise_reduction_type == 0
                and mse_key != self._frozen_nr_ref[0]
                and self._nr_parts.fetch(mse_key) is None
            )
            if fq.noise_reduction_type == 0:                                  # 16
                px_nr = self._reduce_noise_cached(
                    mse_key, px_mse, fq.noise_reduction_amount,
                    fq.noise_whitening_amount, use_frozen=use_frozen)
            else:
                px_nr = px_mse
            if not use_frozen:
                self._nr_cache.put(nr_key, px_nr)
        px_out = self._apply_manual_bezier_lut_cached(                        # 17
            px_nr, ctx.lut_control_points, ctx.output_bit_depth, tuple(p.lut))
        return px_out

    def _enhance_multiscale_cached(self, ctx: PostProcessingContext):
        """14단계(correct_intensity_slope) 및 15단계(enhance_multiscale_detail)를
        동일한 함수/인자 순서로 실행하되, 파라미터 변경과 무관한 중간 단계를 캐시해 재사용한다.
        """
        fq = ctx.frequency_params
        lg = ctx.gradation_params
        h, w = self.s.height, self.s.width

        ds = list(fq.band_sharpness)
        dcs = list(fq.detail_contrast_suppression)
        lcs = list(fq.layer_contrast)
        e_band_ind = [k for k in range(8) if ds[k] > 0]
        assert len(e_band_ind) == 4, f"eBandNum={len(e_band_ind)} != 4 (지원 범위)"
        e_band_key = tuple(e_band_ind)

        # 1. 14단계(기울기 보상) + 15단계 피라미드 분해(decompose_pyramid) 및 대역 차분(img_dl)
        pyr_key = (
            ctx.body_part,
            lg.slope_compensation,
            lg.measurement_field_type,
            ctx.rotation_angle,
            e_band_key,
        )
        pyr = self._pyr_cache.fetch(pyr_key)
        if pyr is None:
            lt = correct_intensity_slope(                                     # 14
                self.log_tone.copy(), self.colli_org.copy(), self.colli_map.copy(),
                ctx.body_part, lg.slope_compensation,
                lg.measurement_field_type, ctx.rotation_angle)
            all_saturated = (float(lt.max()) == float(lt.min()))
            img_bl = decompose_pyramid(e_band_ind, _T_SIGMA, _F_PIXEL_SPACE, lt)
            for k in range(3, -1, -1):
                np.maximum(img_bl[k], 0.0, out=img_bl[k])
            f_img_bl_avr = float(np.mean(img_bl[3], dtype=np.float64))
            if f_img_bl_avr <= 0:
                img_bl[3][:] = 0
            img_dl_raw = [None] * 4
            for k in range(3, 0, -1):
                img_dl_raw[k] = (img_bl[k - 1] - img_bl[k]).astype(np.float32)
            img_dl_raw[0] = (lt - img_bl[0]).astype(np.float32)
            pyr = {
                "all_saturated": all_saturated,
                "f_img_bl_avr": f_img_bl_avr,
                "img_bl3": img_bl[3],
                "img_dl_raw": tuple(img_dl_raw),
            }
            self._pyr_cache.put(pyr_key, pyr)

        if pyr["all_saturated"]:
            fq.noise_reduction_amount = 0

        # 2. Base Layer 계조 매핑 (ROI 평가 → Sigmoid → Tone → Latitude → Contrast LUT)
        roi_key = (
            pyr_key,
            lg.key_feature_percent,
            lg.key_feature_percent_lung,
            lg.key_feature_percent_media,
        )
        sig_key = (roi_key, lg.key_feature_bias)
        tone_key = (sig_key, lcs[1], lcs[2])
        lat_key = (
            tone_key,
            lg.latitude_top_gain,
            lg.latitude_bottom_gain,
            lg.latitude_roi_area,
        )
        base_key = (
            lat_key,
            lg.global_contrast,
            lg.brightness_bias,
        )
        base = self._base_cache.fetch(base_key)
        if base is None:
            if pyr["f_img_bl_avr"] > 0:
                px_img_out = self._lat_cache.fetch(lat_key)
                if px_img_out is None:
                    px_tone = self._tone_cache.fetch(tone_key)
                    if px_tone is None:
                        px_sig = self._sig_cache.fetch(sig_key)
                        if px_sig is None:
                            roi = self._roi_cache.fetch(roi_key)
                            if roi is None:
                                roi = evaluate_roi_statistics(
                                    pyr["img_bl3"], ctx.body_part,
                                    self.colli_org.copy(), self.colli_map.copy(),
                                    self.roi_l.copy(), self.roi_s.copy(), lg)
                                self._roi_cache.put(roi_key, roi)
                            px_sig = apply_center_sigmoid(pyr["img_bl3"], ctx.body_part, roi, lg)
                            self._sig_cache.put(sig_key, px_sig)
                        px_tone = apply_tone_representation(px_sig, fq)
                        self._tone_cache.put(tone_key, px_tone)
                    if not (lg.latitude_top_gain == 0.0 and lg.latitude_bottom_gain == 0.0):
                        px_img_out = compensate_latitude(px_tone, lg)
                    else:
                        px_img_out = px_tone
                    self._lat_cache.put(lat_key, px_img_out)
            else:
                px_img_out = np.zeros((h, w), dtype=PIXEL_DTYPE)

            f_gc = max(0.0, min(60.0, lg.global_contrast)) * 0.01
            f_ty_bias = max(-60.0, min(60.0, float(lg.brightness_bias))) * 0.01
            if f_gc < 0.15:
                f_gc = f_gc * 0.5 + 0.075

            one_pix = 1.0 / PIX_MAX
            i = np.arange(PIX_MAX + 1, dtype=np.float64)
            f_tmp = (i * one_pix - 0.5) * (1.0 + (f_gc - 0.15) * 8) + 0.5 + f_ty_bias
            f_lut = 1.0 / (1.0 + np.exp(-4.0 * (f_tmp - 0.5)))
            p_lut = np.clip((f_lut * PIX_MAX).astype(np.int64), 0, PIX_MAX).astype(PIXEL_DTYPE)

            idx = np.clip(px_img_out.astype(np.int32), 0, PIX_MAX)
            f_density_gain_init = f_lut.astype(np.float32)[idx]
            px_img_out = p_lut[idx]
            base = {
                "px_img_out": px_img_out,
                "f_density_gain": f_density_gain_init,
            }
            self._base_cache.put(base_key, base)

        # DS 임계 정리
        for k in range(4):
            if ds[e_band_ind[k]] <= 0.11:
                ds[e_band_ind[k]] = 0

        # 3. 과대비 억제 1: 분포 기반 (reduce_excess_contrast)
        ecr1_key = (
            pyr_key,
            fq.detail_decay,
            fq.detail_decay_under,
            fq.structure_decay,
            fq.structure_decay_under,
            tuple(fq.detail_contrast_suppression2),
            ctx.tube_line_enhance,
        )
        dl_ecr1 = self._ecr1_cache.fetch(ecr1_key)
        if dl_ecr1 is None:
            dl_ecr1 = [a.copy() for a in pyr["img_dl_raw"]]
            reduce_excess_contrast(fq, self.object_map_os, dl_ecr1, ctx.tube_line_enhance)
            dl_ecr1 = tuple(dl_ecr1)
            self._ecr1_cache.put(ecr1_key, dl_ecr1)

        # contrast_reduction_range 클램프 (0~0.3)
        fq.contrast_reduction_range = max(0.0, min(0.3, fq.contrast_reduction_range))

        # 4. 과대비 억제 2: 베이스 밝기 기반 (reduce_excess_contrast_by_base)
        ecr2_key = (
            base_key,
            ecr1_key,
            lcs[3],
            fq.contrast_reduction_range,
            tuple(fq.contrast_reduction_base),
        )
        ecr2 = self._ecr2_cache.fetch(ecr2_key)
        if ecr2 is None:
            dl_ecr2 = [a.copy() for a in dl_ecr1]
            state = {"TargetY": 0.5}
            reduce_excess_contrast_by_base(
                fq, e_band_ind, ds, lcs, base["px_img_out"], dl_ecr2, state)
            ecr2 = {"img_dl": tuple(dl_ecr2), "target_y": state["TargetY"]}
            self._ecr2_cache.put(ecr2_key, ecr2)

        # 5. 과대비 억제 3: 디테일 합성 밝기 기반 (reduce_excess_contrast_by_detail) + 재구성
        f_density_gain = base["f_density_gain"].copy()
        state = {"TargetY": ecr2["target_y"]}
        reduce_excess_contrast_by_detail(
            fq, e_band_ind, ds, dcs, ecr2["img_dl"], f_density_gain, state)

        px_out = (PIX_MAX * np.clip(f_density_gain, 0.0, 1.0)).astype(PIXEL_DTYPE)
        return px_out, pyr["all_saturated"]

    def _ensure_nr_parts(self, mse_key, px_mse, f_reduction, f_whitening):
        """지정된 mse_key 에 대해 정확한 NR 성분(out_f, noise, nw)을 계산/캐시하여 반환한다."""
        if self._iw_f is None:
            self._iw_f = self.img.astype(np.float32)
        iw_f = self._iw_f
        if self._cov_map is None:                                    # 1. 공분산맵: 입력 원본에만 의존
            self._cov_map = estimate_noise_covariance_map(
                iw_f, g=30, n=1, f=10, nb=3, rc=2, nmax=256)
        if self._nlm_plan is None:                                   # 2. 다중스케일 NLM 가중치 플랜: 입력 원본에만 의존
            self._nlm_plan = build_multiscale_nlm_plan(
                iw_f, self._cov_map, t_base=4, f0=8, n0=1,
                cnlm=0.8, decay=0.6, ksigma=1.0, th=0.9,
                gamma=0.8, correct=4, stair=1, nlev=4)

        parts = self._nr_parts.fetch(mse_key)
        if parts is None:                                            # 3. 디노이즈: 사전 계산된 NLM 플랜 적용
            in_f = px_mse.astype(np.float32)
            out_f = denoise_multiscale_with_plan(in_f, self._nlm_plan)
            parts = {"out_f": out_f, "noise": (in_f - out_f).astype(np.float32), "nw": None}
            self._nr_parts.put(mse_key, parts)

        if f_reduction < 100.0 and f_whitening > 0.0 and parts["nw"] is None:
            parts["nw"] = whiten_noise(parts["noise"], iw_f, g=32, n=1, f=24, rc=2, nn=0,
                                       alpha=0.99, nb=4, nmax=256)
        return parts

    def _reduce_noise_cached(self, mse_key, px_mse, f_reduction, f_whitening,
                             use_frozen: bool = False):
        """noise_reduction.reduce_noise() 와 같은 계산을 단계별로 나눠 중간 결과를 재사용한다."""
        f_reduction = min(f_reduction, 80.0)
        f_whitening = min(max(f_whitening, 0.0), 100.0)
        if f_reduction == 0.0 and f_whitening == 0.0:
            return px_mse.copy()

        if use_frozen and self._frozen_nr_ref is not None:
            # MSE 그룹 탐색 중: 그룹 시작점의 노이즈 잔차를 재사용하여 0.02초 내에 평가
            ref_parts = self._frozen_nr_ref[1]
            noise = ref_parts["noise"]
            out_f = px_mse.astype(np.float32) - noise
            if f_reduction < 100.0 and f_whitening > 0.0 and ref_parts["nw"] is not None:
                w = f_whitening / 100.0
                noise = noise + w * (ref_parts["nw"] - noise)
        else:
            parts = self._ensure_nr_parts(mse_key, px_mse, f_reduction, f_whitening)
            out_f = parts["out_f"]
            noise = parts["noise"]
            if f_reduction < 100.0 and f_whitening > 0.0:
                w = f_whitening / 100.0
                noise = noise + w * (parts["nw"] - noise)

        r1 = 1.0 - f_reduction / 100.0                               # 4. 재합성 (reduce_noise 와 동일)
        result = out_f.astype(np.float64) + r1 * noise.astype(np.float64)
        return np.clip(np.rint(result), np.iinfo(PIXEL_DTYPE).min,
                       np.iinfo(PIXEL_DTYPE).max).astype(PIXEL_DTYPE)

    @property
    def pix_max(self) -> int:
        return PIX_MAX
