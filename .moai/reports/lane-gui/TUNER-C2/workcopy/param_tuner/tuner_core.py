"""UI/CLI 공용 튜닝 작업 (Qt 비의존)."""
import os
import time
from dataclasses import dataclass, field
from typing import List, Optional

import numpy as np

from pe_post.common import BIT_DEPTH, PIX_MAX, PIXEL_DTYPE, RAW_DTYPE
from param_tuner.metrics import LossEvaluator, LossWeights
from param_tuner.optimizer import OptConfig, Optimizer
from param_tuner.param_space import ParamDef, Params
from param_tuner.parallel_eval import ParallelEvaluator, limit_by_memory, make_spec
from param_tuner.pipeline_runner import PipelineRunner, RunSettings
from param_tuner.results import save_results


def load_raw_image(path: str, width: int, height: int) -> np.ndarray:
    a = np.fromfile(path, dtype=RAW_DTYPE)
    if a.size != width * height:
        raise ValueError(f"raw 크기 불일치: {os.path.basename(path)} "
                         f"{a.size} != {width}x{height}={width * height} (bit={BIT_DEPTH})")
    return a.reshape(height, width).astype(PIXEL_DTYPE)


@dataclass
class JobConfig:
    src_path: str = ""
    target_path: str = ""
    width: int = 3040
    height: int = 2466
    object_type: int = 1107          # C++ TObjectType (Chest PA)
    shutter: List[int] = field(default_factory=lambda: [10, 0, 10, 2466, 3026, 2466, 3026, 0])
    collimator_edge_radius: int = 15
    eval_scale: float = 1.0
    weights: LossWeights = field(default_factory=LossWeights)
    opt: OptConfig = field(default_factory=OptConfig)
    final_full_eval: bool = True
    out_dir: Optional[str] = None
    protocol_name: str = ""          # 촬영 프로토콜 이름 (결과 폴더/best_params 에 기록)
    protocol_code: Optional[int] = None   # 프로토콜 번호 (기록용)

    def run_settings(self) -> RunSettings:
        return RunSettings(self.width, self.height, self.object_type,
                           np.array(self.shutter, dtype=np.int32).reshape(4, 2),
                           self.collimator_edge_radius)

    def to_dict(self) -> dict:
        return {"src_path": self.src_path, "target_path": self.target_path,
                "width": self.width, "height": self.height, "bit_depth": BIT_DEPTH,
                "protocol": self.protocol_name, "protocol_code": self.protocol_code, "object_type": self.object_type,
                "shutter": self.shutter, "eval_scale": self.eval_scale,
                "weights": vars(self.weights), "opt": vars(self.opt)}


_runner_cache = {}


def _get_runner(cfg: JobConfig, src, log) -> PipelineRunner:
    key = (cfg.src_path, os.path.getmtime(cfg.src_path), cfg.width, cfg.height,
           cfg.object_type, tuple(cfg.shutter), cfg.collimator_edge_radius, BIT_DEPTH)
    r = _runner_cache.get(key)
    if r is None:
        _runner_cache.clear()
        r = PipelineRunner(src, cfg.run_settings(), log)
        _runner_cache[key] = r
    r.log = log
    return r


class TuneJob:
    def __init__(self, cfg: JobConfig, log=None, on_eval=None, on_stage=None, should_stop=None):
        self.cfg = cfg
        self.log = log or print
        self.on_eval = on_eval or (lambda info: None)
        self.on_stage = on_stage or (lambda t: None)
        self.should_stop = should_stop or (lambda: False)
        self.src = self.target = self.runner = None

    def load(self):
        c = self.cfg
        self.src = load_raw_image(c.src_path, c.width, c.height)
        self.target = load_raw_image(c.target_path, c.width, c.height)
        self.runner = _get_runner(c, self.src, self.log)

    def _evaluator(self, scale: float) -> LossEvaluator:
        if not self.runner.ready:
            raise RuntimeError("prepare() 필요")
        return LossEvaluator(self.target, PIX_MAX, self.runner.colli_map, scale, self.cfg.weights)

    def run_single(self, p: Params):
        """1회 처리 → (out, loss, detail)."""
        self.load()
        if not self.runner.ready:
            self.runner.prepare(p)
        ev = self._evaluator(self.cfg.eval_scale)
        out = self.runner.run(p)
        loss, detail = ev.evaluate(out)
        return out, loss, detail

    def run(self, init: Params, defs: List[ParamDef]) -> dict:
        c = self.cfg
        t0 = time.time()
        self.load()
        if not self.runner.ready:
            self.runner.prepare(init)
        ev = self._evaluator(c.eval_scale)

        def evaluate(p: Params):
            out = self.runner.run(p)
            loss, detail = ev.evaluate(out)
            return loss, detail, out

        pool, batch_fn = None, None
        if c.opt.workers > 1:                                   # 병렬 평가 (tone/freq/suppress 그룹)
            n = limit_by_memory(c.opt.workers)                  # 작업자당 약 1.5GB → 가용 메모리에 맞춤
            if n < c.opt.workers:
                self.log(f"[병렬] 가용 메모리 기준으로 작업자 {c.opt.workers} → {n} 개로 줄임")
            c.opt.workers = n
            if n > 1:
                pool = ParallelEvaluator(make_spec(c, init, BIT_DEPTH), n)
                batch_fn = lambda plist, thr: pool.evaluate_batch(plist, thr, self.should_stop)
                self.log(f"[병렬] 평가 작업자 {n}개 시작 (각 작업자가 영상을 불러오고 준비하는 데 수 초 소요)")
        try:
            opt = Optimizer(evaluate, defs, init, c.opt, self.on_eval, self.on_stage, self.should_stop,
                            batch_eval_fn=batch_fn,
                            begin_mse_group=self.runner.begin_mse_group,
                            end_mse_group=self.runner.end_mse_group)
            best = opt.run()
        finally:
            if pool is not None:
                pool.close()

        out = self.runner.run(best)
        detail = dict(opt.best_detail)
        if c.final_full_eval and c.eval_scale != 1.0:
            self.on_stage("최종 전체 해상도 평가")
            _, detail = self._evaluator(1.0).evaluate(out)
            detail["note"] = "full-res"
        detail["evals"] = opt.n_evals
        detail["seconds"] = round(time.time() - t0, 1)
        out_dir = save_results(best, out, self.target, PIX_MAX, BIT_DEPTH, detail,
                               opt.history, c.to_dict(), c.out_dir)
        self.log(f"결과 저장: {out_dir}")
        return {"best": best, "out": out, "detail": detail, "out_dir": out_dir,
                "history": opt.history}
