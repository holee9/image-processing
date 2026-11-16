"""그룹 단계별 좌표 하강(coordinate descent) + Hooke-Jeeves 패턴 이동 최적화.

그룹 순서(tone → freq → suppress → noise → lut)대로, 그룹 내 파라미터를 하나씩
±step 으로 탐색한다. 개선되면 같은 방향으로 계속 진행(line search)하고, 한 sweep 에서
2개 이상의 파라미터가 함께 움직이면 대각선 방향(Δp = p_after - p_before)으로
Hooke-Jeeves 패턴 이동(pattern move)을 수행하여 강하게 결합된 변수(밝기↔대비,
선예도↔과대비 억제, LUT 제어점 등)의 대각선 골짜기를 빠르게 따라간다.
개선이 없으면 step 을 절반으로 줄이고 step=1 에서도 개선이 없으면 해당 그룹 종료.
rounds > 1 이면 전체 그룹을 다시 한 번 돈다.
파이프라인은 미분 불가이므로 derivative-free 방식만 사용한다.

병렬 모드(batch_eval_fn 제공 시): tone/freq/suppress 그룹은 한 sweep 의 모든 후보(각 파라미터 ±step)를
여러 프로세스에서 동시에 평가한 뒤, 개선된 이동들의 조합(combo)과 2배 연장(line search / pattern extension)
후보들을 한 배치로 묶어 병렬 평가한다.
noise/lut 그룹은 NR/LUT 캐시 덕에 메인 프로세스 순차 평가가 더 빠르므로 순차 방식을 쓴다.
"""
import time
from dataclasses import dataclass
from typing import Callable, Dict, List, Optional

from param_tuner.param_space import GROUP_ORDER, GROUP_TITLES, ParamDef, Params


@dataclass
class OptConfig:
    max_evals: int = 800
    max_seconds: float = 1800.0
    target_loss: float = 1e-4        # 이 값 이하면 종료
    min_rel_improve: float = 1e-3    # 한 sweep 의 상대 개선이 이보다 작으면 그룹 종료
    rounds: int = 1
    workers: int = 1                 # 병렬 평가 작업자 수 (1 = 순차)


class Stopped(Exception):
    pass


PARALLEL_GROUPS = ("tone", "freq", "suppress")     # 후보마다 MSE 를 새로 계산하는(= 병렬 이득이 큰) 그룹

# 그룹별 물리적 결합도가 높은 파라미터 쌍 (단일 축 step=1 탐색이 대각선 골짜기 벽에 막힐 때 대각선 ±1 이동 시도)
_COUPLED_PAIRS = {
    "tone": (("post.0", "post.1"), ("post.3", "post.13"), ("post.2", "post.3"), ("post.8", "post.10")),
    "freq": (("post.14", "post.15"), ("post.16", "post.17"), ("post.19", "post.20")),
    "suppress": (("post.28", "post.30"), ("post.29", "post.31"), ("post.6", "post.37"), ("post.34", "post.35")),
    "noise": (("post.23", "post.26"),),
    "lut": (("lut.0", "lut.1"), ("lut.3", "lut.4"), ("lut.6", "lut.7"), ("lut.9", "lut.10")),
}


def _init_steps(defs: List[ParamDef], rnd: int = 0) -> Dict[str, int]:
    """파라미터별 초기 탐색 보폭(step) 산출.
    - Manual LUT 는 단조 제약으로 인한 ±12, ±6 낭비를 막기 위해 상한 4 적용.
    - 2회차 Iteration(rnd=1)은 이미 수렴된 근방이므로 상한 4, 3회차 이상(rnd>=2)은 상한 2 적용."""
    round_cap = None if rnd <= 0 else (4 if rnd == 1 else 2)
    steps = {}
    for d in defs:
        st = max(1, round((d.hi - d.lo) / 8))
        if d.kind == "lut":
            st = min(st, 4)
        if round_cap is not None:
            st = min(st, round_cap)
        steps[d.key] = st
    return steps


class Optimizer:
    """callbacks:
        on_eval(info)   : 평가마다 호출 (info: n, loss, detail, params, is_best, out)
        on_stage(text)  : 단계 안내
        should_stop()   : True 면 중지
    """

    def __init__(self, evaluate_fn: Callable[[Params], tuple], defs: List[ParamDef],
                 init: Params, cfg: OptConfig,
                 on_eval=None, on_stage=None, should_stop=None, batch_eval_fn=None,
                 begin_mse_group: Optional[Callable[[Params], None]] = None,
                 end_mse_group: Optional[Callable[[], None]] = None):
        self.evaluate_fn = evaluate_fn      # Params -> (loss, detail, out_img)
        # (list[Params], thr) -> list[(loss, detail, out|None)] | None(중지). loss < thr 인 후보만 out 을 돌려준다.
        self.batch_eval_fn = batch_eval_fn
        self.begin_mse_group = begin_mse_group
        self.end_mse_group = end_mse_group
        self.defs = [d for d in defs if d.enabled]
        self.cfg = cfg
        self.on_eval = on_eval or (lambda info: None)
        self.on_stage = on_stage or (lambda t: None)
        self.should_stop = should_stop or (lambda: False)
        self.best = init.copy()
        self.best_loss = float("inf")
        self.best_detail: Dict = {}
        self.n_evals = 0           # 실제 파이프라인 평가 횟수
        self.history = []          # (n, loss, params key)
        self._memo: Dict[tuple, float] = {}
        self._pref_sign: Dict[str, int] = {}  # 파라미터별 직전 개선 방향(+1 또는 -1) 기억
        self._t0 = time.time()

    # ------------------------------------------------------------------
    def _budget_exhausted(self) -> bool:
        return (self.n_evals >= self.cfg.max_evals
                or time.time() - self._t0 > self.cfg.max_seconds
                or self.best_loss <= self.cfg.target_loss)

    def _eval(self, p: Params) -> float:
        k = p.key()
        if k in self._memo:
            return self._memo[k]
        if self.should_stop():
            raise Stopped()
        loss, detail, out = self.evaluate_fn(p)
        self._register(p, loss, detail, out)
        return loss

    def _register(self, p: Params, loss: float, detail: dict, out):
        """평가 결과 기록 (best/이력 갱신 + 진행 콜백). out 은 best 일 때만 의미가 있다."""
        k = p.key()
        self.n_evals += 1
        self._memo[k] = loss
        is_best = loss < self.best_loss
        if is_best:
            self.best, self.best_loss, self.best_detail = p.copy(), loss, detail
        self.history.append((self.n_evals, loss, k))
        self.on_eval({"n": self.n_evals, "loss": loss, "detail": detail, "params": p,
                      "is_best": is_best, "out": out if is_best else None,
                      "best_loss": self.best_loss})

    @staticmethod
    def _is_inactive_nr_param(p: Params, d: ParamDef) -> bool:
        """NR_SELECTION(post[24]) != 0 이면 NR 강도(23)·백색화(26)는 파이프라인에서 무시됨."""
        return d.kind == "post" and d.index in (23, 26) and p.post[24] != 0

    def _try(self, d: ParamDef, v: int) -> bool:
        """best 에서 d 를 v 로 바꿔 평가, 개선되면 best 갱신하고 True."""
        if self._is_inactive_nr_param(self.best, d):
            return False
        v = max(d.lo, min(d.hi, v))
        if v == self.best.get(d):
            return False
        cand = self.best.copy()
        cand.set(d, v)
        if d.kind == "lut" and not cand.lut_valid():
            return False
        before = self.best_loss
        self._eval(cand)
        return self.best_loss < before

    def _try_pair(self, d1: ParamDef, v1: int, d2: ParamDef, v2: int) -> bool:
        """결합된 두 파라미터 d1, d2 를 동시에 변경해 평가하고 개선되면 best 갱신 후 True."""
        if self._is_inactive_nr_param(self.best, d1) or self._is_inactive_nr_param(self.best, d2):
            return False
        v1 = max(d1.lo, min(d1.hi, v1))
        v2 = max(d2.lo, min(d2.hi, v2))
        if v1 == self.best.get(d1) and v2 == self.best.get(d2):
            return False
        cand = self.best.copy()
        cand.set(d1, v1)
        cand.set(d2, v2)
        if (d1.kind == "lut" or d2.kind == "lut") and not cand.lut_valid():
            return False
        before = self.best_loss
        self._eval(cand)
        return self.best_loss < before

    def _try_coupled_pairs(self, group: str, defs: List[ParamDef]) -> bool:
        """단일 축 step=1 이동이 멈췄을 때 물리적으로 결합된 파라미터 쌍을 대각선(±1, ±1)으로 탐색."""
        by_key = {d.key: d for d in defs if not self._is_inactive_nr_param(self.best, d)}
        any_moved = False
        for k1, k2 in _COUPLED_PAIRS.get(group, ()):
            if k1 not in by_key or k2 not in by_key:
                continue
            d1, d2 = by_key[k1], by_key[k2]
            for s1, s2 in ((+1, -1), (-1, +1), (+1, +1), (-1, -1)):
                if self._budget_exhausted():
                    return any_moved
                moved = False
                while not self._budget_exhausted() and \
                        self._try_pair(d1, self.best.get(d1) + s1, d2, self.best.get(d2) + s2):
                    moved = True
                    any_moved = True
                if moved:
                    break
        return any_moved

    def _pattern_move(self, defs: List[ParamDef], sweep_base: Params):
        """Hooke-Jeeves 패턴 이동: 한 sweep 에서 2개 이상의 파라미터가 변경되었으면
        합성 변위 벡터 Δp = best - sweep_base 방향으로 개선이 계속되는 동안 전진한다."""
        deltas = [(d, self.best.get(d) - sweep_base.get(d))
                  for d in defs if self.best.get(d) != sweep_base.get(d)]
        if len(deltas) < 2:
            return
        has_lut = any(d.kind == "lut" for d, _ in deltas)
        while not self._budget_exhausted():
            pat = self.best.copy()
            changed = False
            for d, delta in deltas:
                if self._is_inactive_nr_param(pat, d):
                    continue
                nv = max(d.lo, min(d.hi, self.best.get(d) + delta))
                if nv != self.best.get(d):
                    pat.set(d, nv)
                    changed = True
            if not changed or (has_lut and not pat.lut_valid()):
                break
            before = self.best_loss
            self._eval(pat)
            if self.best_loss >= before:
                break

    # ------------------------------------------------------------------
    def _tune_group(self, group: str, defs: List[ParamDef], rnd: int = 0):
        # noise 그룹에서는 NR_SELECTION(24)을 먼저 판정한 뒤 NR 강도(23)/백색화(26)를 탐색
        if group == "noise":
            defs = sorted(defs, key=lambda d: (0 if (d.kind == "post" and d.index == 24) else 1, d.index))
        steps = _init_steps(defs, rnd)
        while not self._budget_exhausted():
            sweep_start = self.best_loss
            sweep_base = self.best.copy()
            # 이번 sweep 을 모든 활성 파라미터 step=1 로 수행하는지 (sweep 도중 줄어든 step 은 다음 sweep 에서 시도됨)
            was_fine = all(steps[d.key] == 1 or self._is_inactive_nr_param(self.best, d) for d in defs)
            for d in defs:
                if self._budget_exhausted():
                    return
                if self._is_inactive_nr_param(self.best, d):
                    steps[d.key] = 1
                    continue
                st = steps[d.key]
                moved = False
                pref = self._pref_sign.get(d.key, +1)
                for sign in (pref, -pref):
                    while not self._budget_exhausted() and \
                            self._try(d, self.best.get(d) + sign * st):
                        moved = True
                        self._pref_sign[d.key] = sign
                    if moved:
                        break
                if not moved and st > 1:
                    steps[d.key] = max(1, st // 2)

            # 결합 변수 대각선 탐색 (Hooke-Jeeves Pattern Move)
            if not self._budget_exhausted():
                self._pattern_move(defs, sweep_base)

            improved = sweep_start - self.best_loss
            # step=1 로 수행한 sweep 에서도 개선이 미미하면 결합 쌍 대각선 탐색을 시도한 뒤 종료 여부 결정
            if was_fine and improved <= self.cfg.min_rel_improve * max(sweep_start, 1e-12):
                if not self._budget_exhausted() and self._try_coupled_pairs(group, defs):
                    improved = sweep_start - self.best_loss
                if improved <= self.cfg.min_rel_improve * max(sweep_start, 1e-12):
                    return

    def _evaluate_batch(self, plist: List[Params]):
        """plist 를 병렬 평가해 기록한다 (best 이하 후보는 out 포함). 중지되면 Stopped."""
        if self.should_stop():
            raise Stopped()
        res = self.batch_eval_fn(plist, self.best_loss)
        if res is None:
            raise Stopped()
        for p, (loss, detail, out) in zip(plist, res):
            self._register(p, loss, detail, out)

    def _tune_group_parallel(self, group: str, defs: List[ParamDef], rnd: int = 0):
        steps = _init_steps(defs, rnd)
        while not self._budget_exhausted():
            sweep_start = self.best_loss
            was_fine = all(steps[d.key] == 1 for d in defs)
            base, base_loss = self.best.copy(), self.best_loss

            # 1) 후보: base 에서 각 파라미터를 ±step 으로 바꾼 것 (모두 동시에 평가)
            cands = []
            for d in defs:
                for sign in (+1, -1):
                    v = max(d.lo, min(d.hi, base.get(d) + sign * steps[d.key]))
                    if v == base.get(d):
                        continue
                    c = base.copy()
                    c.set(d, v)
                    if d.kind == "lut" and not c.lut_valid():
                        continue
                    cands.append((d, v, sign, c))
            todo = [c for _, _, _, c in cands if c.key() not in self._memo]
            todo = todo[:max(0, self.cfg.max_evals - self.n_evals)]       # 평가 횟수 한도 내에서만
            if todo:
                self._evaluate_batch(todo)

            # 2) 파라미터별 최선의 개선 이동 수집 → 조합(combo), 대각선 2배 패턴 이동(combo_2x),
            #    상위 단일 파라미터 2배 연장(line search) 후보들을 한 배치로 병렬 평가
            moves = {}
            for d, v, sign, c in cands:
                loss = self._memo.get(c.key())
                if loss is not None and loss < base_loss and (d.key not in moves or loss < moves[d.key][0]):
                    moves[d.key] = (loss, d, v, sign)

            if moves and not self._budget_exhausted():
                ext_cands = []
                seen_keys = set(self._memo.keys())

                def _add_ext(cand_p: Params, check_lut: bool):
                    k = cand_p.key()
                    if k not in seen_keys and (not check_lut or cand_p.lut_valid()):
                        seen_keys.add(k)
                        ext_cands.append(cand_p)

                has_lut = any(d.kind == "lut" for _, d, _, _ in moves.values())
                if len(moves) > 1:
                    combo = base.copy()
                    combo_2x = base.copy()
                    for _, d, v, _ in moves.values():
                        combo.set(d, v)
                        delta = v - base.get(d)
                        combo_2x.set(d, max(d.lo, min(d.hi, base.get(d) + 2 * delta)))
                    _add_ext(combo, has_lut)
                    _add_ext(combo_2x, has_lut)

                # 상위 개선 단일 파라미터에 대해 2×step 직선 탐색(line search) 후보 추가
                top_single = sorted(moves.values(), key=lambda item: item[0])[:max(2, self.cfg.workers)]
                for _, d, v, sign in top_single:
                    v2 = max(d.lo, min(d.hi, base.get(d) + 2 * sign * steps[d.key]))
                    if v2 != v:
                        c2 = base.copy()
                        c2.set(d, v2)
                        _add_ext(c2, d.kind == "lut")

                ext_cands = ext_cands[:max(0, self.cfg.max_evals - self.n_evals)]
                if ext_cands:
                    self._evaluate_batch(ext_cands)

            # 3) 개선 이동이 없던 파라미터는 step 을 절반으로
            for d in defs:
                if d.key not in moves and steps[d.key] > 1:
                    steps[d.key] = max(1, steps[d.key] // 2)
            improved = sweep_start - self.best_loss
            if was_fine and improved <= self.cfg.min_rel_improve * max(sweep_start, 1e-12):
                if not self._budget_exhausted():
                    by_key = {d.key: d for d in defs}
                    pair_cands = []
                    seen_keys = set(self._memo.keys())
                    cur = self.best
                    for k1, k2 in _COUPLED_PAIRS.get(group, ()):
                        if k1 not in by_key or k2 not in by_key:
                            continue
                        d1, d2 = by_key[k1], by_key[k2]
                        for s1, s2 in ((+1, -1), (-1, +1), (+1, +1), (-1, -1)):
                            v1 = max(d1.lo, min(d1.hi, cur.get(d1) + s1))
                            v2 = max(d2.lo, min(d2.hi, cur.get(d2) + s2))
                            if v1 == cur.get(d1) and v2 == cur.get(d2):
                                continue
                            cp = cur.copy()
                            cp.set(d1, v1)
                            cp.set(d2, v2)
                            pk = cp.key()
                            if pk not in seen_keys and (not (d1.kind == "lut" or d2.kind == "lut") or cp.lut_valid()):
                                seen_keys.add(pk)
                                pair_cands.append(cp)
                    pair_cands = pair_cands[:max(0, self.cfg.max_evals - self.n_evals)]
                    if pair_cands:
                        self._evaluate_batch(pair_cands)
                        improved = sweep_start - self.best_loss
                if improved <= self.cfg.min_rel_improve * max(sweep_start, 1e-12):
                    return

    def run(self) -> Params:
        # 0) 초기(default) 파라미터 평가
        self.on_stage("초기 파라미터 평가")
        self._eval(self.best)
        try:
            for rnd in range(self.cfg.rounds):
                for g in GROUP_ORDER:
                    defs = [d for d in self.defs if d.group == g]
                    if not defs or self._budget_exhausted():
                        continue
                    parallel = bool(self.batch_eval_fn) and self.cfg.workers > 1 and g in PARALLEL_GROUPS
                    self.on_stage(f"Iteration {rnd + 1}/{self.cfg.rounds} - {GROUP_TITLES[g]} "
                                  f"({len(defs)}개 파라미터{f', 병렬 {self.cfg.workers}' if parallel else ''})")
                    use_mse_freeze = (
                        not parallel
                        and g in PARALLEL_GROUPS
                        and self.begin_mse_group is not None
                        and self.end_mse_group is not None
                        and self.best.post[24] == 0
                        and (self.best.post[23] > 0 or self.best.post[26] > 0)
                    )
                    if use_mse_freeze:
                        grp_best = self.best.copy()
                        grp_loss = self.best_loss
                        grp_detail = dict(self.best_detail)
                        pre_memo_keys = set(self._memo.keys())
                        self.begin_mse_group(grp_best)
                        try:
                            self._tune_group(g, defs, rnd)
                        finally:
                            self.end_mse_group()
                            for mk in list(self._memo.keys()):
                                if mk not in pre_memo_keys:
                                    del self._memo[mk]
                        if self.best.key() != grp_best.key() and not self.should_stop():
                            cand_best = self.best.copy()
                            exact_loss, exact_detail, _ = self.evaluate_fn(cand_best)
                            self._memo[cand_best.key()] = exact_loss
                            if exact_loss <= grp_loss:
                                self.best = cand_best
                                self.best_loss = exact_loss
                                self.best_detail = exact_detail
                            else:
                                self.best = grp_best
                                self.best_loss = grp_loss
                                self.best_detail = grp_detail
                    elif parallel:
                        self._tune_group_parallel(g, defs, rnd)
                    else:
                        self._tune_group(g, defs, rnd)
                if self._budget_exhausted():
                    if self.n_evals >= self.cfg.max_evals:
                        self.on_stage(f"최대 평가수({self.cfg.max_evals}회) 도달로 탐색 종료 "
                                      f"(Iteration {rnd + 1}/{self.cfg.rounds})")
                    elif time.time() - self._t0 > self.cfg.max_seconds:
                        self.on_stage(f"최대 제한 시간 도달로 탐색 종료 (Iteration {rnd + 1}/{self.cfg.rounds})")
                    break
        except Stopped:
            self.on_stage("사용자 중지")
        return self.best
