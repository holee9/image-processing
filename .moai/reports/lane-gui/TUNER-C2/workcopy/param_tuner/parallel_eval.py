"""평가(파이프라인 실행 + loss 계산)를 여러 프로세스에서 병렬로 수행한다.

- 평가들은 서로 독립적이라 후보 파라미터를 여러 작업자 프로세스에 나눠 동시에 평가할 수 있다.
- 작업자마다 자기 영상/캐시(PipelineRunner, LossEvaluator)를 가진다 (spawn 방식, Windows/exe 호환).
- 이 모듈은 import 시점에 pe_post 를 import 하지 않는다 (작업자 초기화에서 비트 심도 확인 후 import).
"""
import ctypes
import os
import time
import traceback
import multiprocessing as mp

_W = {}                       # 작업자 프로세스 전역 상태


# ----------------------------------------------------------------------
# 작업자 수 결정
# ----------------------------------------------------------------------
def available_ram_gb() -> float:
    """사용 가능한 물리 메모리(GB). 확인할 수 없으면 보수적으로 8."""
    class _MemStatus(ctypes.Structure):
        _fields_ = [("dwLength", ctypes.c_ulong), ("dwMemoryLoad", ctypes.c_ulong),
                    ("ullTotalPhys", ctypes.c_ulonglong), ("ullAvailPhys", ctypes.c_ulonglong),
                    ("ullTotalPageFile", ctypes.c_ulonglong), ("ullAvailPageFile", ctypes.c_ulonglong),
                    ("ullTotalVirtual", ctypes.c_ulonglong), ("ullAvailVirtual", ctypes.c_ulonglong),
                    ("ullAvailExtendedVirtual", ctypes.c_ulonglong)]
    try:
        ms = _MemStatus()
        ms.dwLength = ctypes.sizeof(_MemStatus)
        ctypes.windll.kernel32.GlobalMemoryStatusEx(ctypes.byref(ms))
        return ms.ullAvailPhys / 2 ** 30
    except Exception:
        return 8.0


def default_workers() -> int:
    """기본 작업자 수: 논리 코어의 절반 (4코어 이상이면 최소 2, 최대 8, 가용 메모리 한도 반영)."""
    cpus = os.cpu_count() or 2
    n = 1 if cpus <= 2 else min(8, max(2, cpus // 2))
    return limit_by_memory(n)


def limit_by_memory(n: int, per_worker_gb: float = 1.5) -> int:
    """작업자 1개당 약 1.5GB(영상 + 중간 캐시)로 보고 사용 가능 메모리에 맞게 줄인다."""
    return max(1, min(n, int(available_ram_gb() // per_worker_gb)))


# ----------------------------------------------------------------------
# 작업자 프로세스
# ----------------------------------------------------------------------
def _init_worker(spec: dict):
    """작업자 초기화: 영상 로드 + 1~13단계 준비. 실패해도 프로세스가 죽지 않게 오류를 저장해 둔다."""
    try:
        import sys
        root_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
        if root_dir not in sys.path:
            sys.path.insert(0, root_dir)
        os.environ["PE_BIT_DEPTH"] = str(spec["bit"])
        import cv2
        cv2.setNumThreads(1)                                  # 작업자끼리 CPU 를 다투지 않도록
        import numpy as np
        from pe_post.common import BIT_DEPTH, PIX_MAX
        if BIT_DEPTH != spec["bit"]:
            raise RuntimeError(f"작업자 비트 심도 불일치: {BIT_DEPTH} != {spec['bit']}")
        from param_tuner.metrics import LossEvaluator, LossWeights
        from param_tuner.param_space import Params
        from param_tuner.pipeline_runner import PipelineRunner, RunSettings
        from param_tuner.tuner_core import load_raw_image
        w, h = spec["width"], spec["height"]
        src = load_raw_image(spec["src"], w, h)
        tgt = load_raw_image(spec["target"], w, h)
        runner = PipelineRunner(src, RunSettings(w, h, spec["object_type"],
                                                 np.array(spec["shutter"], dtype=np.int32).reshape(4, 2),
                                                 spec["radius"]))
        runner.prepare(Params(list(spec["init_post"]), list(spec["init_lut"])))
        _W.update(runner=runner, Params=Params,
                  ev=LossEvaluator(tgt, PIX_MAX, runner.colli_map, spec["scale"], LossWeights(**spec["weights"])))
    except BaseException:
        _W["error"] = traceback.format_exc()


def _eval_task(post, lut, thr):
    """후보 1개 평가. loss 가 thr 보다 작을 때만 출력 영상을 돌려준다 (프로세스 간 전송량 절약)."""
    if "error" in _W:
        raise RuntimeError("작업자 초기화 실패:\n" + _W["error"])
    p = _W["Params"](list(post), list(lut))
    out = _W["runner"].run(p)
    loss, detail = _W["ev"].evaluate(out)
    return loss, detail, (out if loss < thr else None)


# ----------------------------------------------------------------------
# 메인 프로세스
# ----------------------------------------------------------------------
class ParallelEvaluator:
    """후보 목록을 작업자 풀에서 병렬 평가한다."""

    def __init__(self, spec: dict, n_workers: int):
        # 자식 프로세스(spawn)가 어느 작업 디렉터리에서 시작되든 param_tuner, pe_post 를 찾을 수 있도록 보장
        root_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
        cur_pp = os.environ.get("PYTHONPATH", "")
        if root_dir not in cur_pp.split(os.pathsep):
            os.environ["PYTHONPATH"] = root_dir + (os.pathsep + cur_pp if cur_pp else "")
        os.environ["PE_BIT_DEPTH"] = str(spec["bit"])
        # 작업자 내부 수치 라이브러리가 코어를 과점하지 않도록 (자식 프로세스가 환경변수를 상속)
        for k in ("OMP_NUM_THREADS", "MKL_NUM_THREADS", "OPENBLAS_NUM_THREADS"):
            os.environ[k] = "1"
        self.n = n_workers
        self.pool = mp.get_context("spawn").Pool(n_workers, initializer=_init_worker, initargs=(spec,))

    def evaluate_batch(self, plist, thr: float, should_stop=lambda: False):
        """plist(Params 목록) 평가 → [(loss, detail, out|None), ...] (입력 순서). 중지되면 None 반환."""
        futs = [self.pool.apply_async(_eval_task, (p.post, p.lut, thr)) for p in plist]
        results = [None] * len(futs)
        pending = set(range(len(futs)))
        while pending:
            if should_stop():
                self.close()
                return None
            for i in list(pending):
                if futs[i].ready():
                    results[i] = futs[i].get()           # 작업자 예외는 여기서 다시 발생
                    pending.discard(i)
            if pending:
                time.sleep(0.02)
        return results

    def close(self):
        if self.pool is not None:
            self.pool.terminate()
            self.pool.join()
            self.pool = None


def make_spec(cfg, init_params, bit: int) -> dict:
    """JobConfig + 초기 파라미터 → 작업자 초기화 정보 (pickle 가능한 단순 값만)."""
    return {"src": cfg.src_path, "target": cfg.target_path, "width": cfg.width, "height": cfg.height,
            "object_type": cfg.object_type, "shutter": list(cfg.shutter),
            "radius": cfg.collimator_edge_radius, "scale": cfg.eval_scale, "weights": dict(vars(cfg.weights)),
            "init_post": list(init_params.post), "init_lut": list(init_params.lut), "bit": bit}
