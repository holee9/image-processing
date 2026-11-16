# QA-A-340: the frozen line-residual metric study (design QA-A-339, editions 1..8, later editions override).
# Subcommands:
#   geom  <locked.json> <out_dir>                       7-4 pre-check: per set the admissible positions per part (masks + geometry only, no pixels)
#   pipe  <locked.json> <set>                           shipped pipeline output (kCfgFinal) of the set's two eval frames -> tmp/<set>/ev{0,1}.f32
#   check <out>                                         equivalence checks (C0 vs xpe_bpm_eval QA-A-331 json; injection/C shortcut vs literal)
#   run   <locked.json> <side> <out_dir>                null draws + injection for every frame of one side (one seed-339 stream per side)
#   contam <locked.json> <out>                          known-contamination control on cyan_test Bright_29
import sys, json, os, math, ctypes as C
import numpy as np

W = H = 3072
X0, Y0, X1, Y1 = 6, 2, W - 1, H - 1          # ROI: margins left 6, top 2, right 1, bottom 1 (xpe_bpm_eval.cpp:2420-2421)
R = 3                                        # C estimation radius, fixed (design 5판 §1)
MIN_PAIRS = 100000                           # design 3판 R4
TMP = 'D:/workspace-github/xpe-data/qa-340-tmp'
BIN = 'D:/workspace-github/xpe-pre/build/ci-preprocess/bin'
TRUTH = 'D:/workspace-github/xpe-data/bpm-eval-252f/inj_d/truth.tsv'
CFG_FINAL = b'{"bypassTemp":true,"bypassNonlinearity":true,"bypassBinning":true}'   # kCfgFinal, xpe_bpm_eval.cpp:2343
A_LIST = (2, 3, 4, 5)


# ------------------------------------------------------------------ geometry (design 8판: lineIndexInArea states, xpe_bpm_eval_stats.hpp:183-216)
def truth_items():
    return [l.rstrip('\n').split('\t') for l in open(TRUTH) if l[:1].isdigit()]


def area_status(x, y, w, h, row, area=(X0, Y0, X1, Y1)):
    ax0, ay0, ax1, ay1 = area
    k = h if row else w
    first = y if row else x
    acLo, acHi = (ay0, ay1) if row else (ax0, ax1)
    alLo, alHi = (ax0, ax1) if row else (ay0, ay1)
    p0, ln = (x, w) if row else (y, h)
    if first < acLo or first + k > acHi: return 'ItemOutside', None
    if first - 1 < acLo or first + k >= acHi: return 'ReferenceOutside', None
    lo, hi = max(p0, alLo), min(p0 + ln, alHi)
    if hi <= lo: return 'OutsideArea', None
    if hi - lo < 8: return 'TooShort', None
    if hi - lo < k: return 'LengthBelowThickness', None
    used = (lo, y, hi - lo, h) if row else (x, lo, w, hi - lo)
    return ('Full' if (lo == p0 and hi == p0 + ln) else 'Clipped'), used


def parts_table():
    """The 142 parts in xpe_bpm_eval correct order (line/control items, rects in order, plain-skipped ones dropped: xpe_bpm_eval.cpp:2486-2494)."""
    out = []
    for it in truth_items():
        if it[1] not in ('line', 'control'): continue
        for r, q in enumerate(it[5].split(';')):
            x, y, w, h = map(int, q.split(','))
            row = w > h
            k, L, first = (h, w, y) if row else (w, h, x)
            if first - 1 < 0 or first + k >= (H if row else W) or L < 8: continue        # lineIndexOriented skipped
            st, used = area_status(x, y, w, h, row)
            out.append(dict(idx=len(out), id=int(it[0]), part=r, rect=(x, y, w, h), row=row, k=k, status=st, used=used,
                            L=(used[2] if row else used[3]) if used else None))
    return out


def line_masks(lines_tsv):
    """Row/column index sets (reading B) of truth line items +-3 and of the defect map's line and candidate records +-3."""
    rs_t, cs_t = np.zeros(H, bool), np.zeros(W, bool)
    for it in truth_items():
        if it[1] not in ('line', 'control'): continue
        for q in it[5].split(';'):
            x, y, w, h = map(int, q.split(','))
            if w > h: rs_t[max(0, y - 3):y + h + 3] = True       # rowSkip: y-3 .. y+h+2 (xpe_bpm_eval.cpp:2542)
            else: cs_t[max(0, x - 3):x + w + 3] = True
    rs_m, cs_m = np.zeros(H, bool), np.zeros(W, bool)
    n = {'line': 0, 'candidate': 0}
    for l in open(lines_tsv).read().splitlines()[1:]:
        f = l.split('\t')
        if f[0] not in n: continue
        n[f[0]] += 1
        for s in f[12].split(';') if f[12] else []:
            ln = int(s.split(':')[0])
            if f[1] == 'row': rs_m[max(0, ln - 3):ln + 4] = True
            else: cs_m[max(0, ln - 3):ln + 4] = True
    return rs_t, cs_t, rs_m, cs_m, n


def admissible(p, rs, cs):
    """All Delta != 0 in the part's own direction whose moved used rect is Full/Clipped in the ROI (same clipping as 8판) and whose
    rows (row part) / columns (column part) +-3 avoid the masked rows / columns (7-2 reading B). Returns the list of moved used rects."""
    x, y, w, h = p['rect']
    out = []
    for d in range(-W, W + 1):
        if d == 0: continue
        nx, ny = (x, y + d) if p['row'] else (x + d, y)
        st, used = area_status(nx, ny, w, h, p['row'])
        if used is None: continue
        if p['row']:
            if rs[max(0, ny - 3):ny + h + 3].any(): continue
        elif cs[max(0, nx - 3):nx + w + 3].any(): continue
        out.append(used)
    return out


# ------------------------------------------------------------------ line index C0 (numpy port of lineIndexOriented, xpe_bpm_eval_stats.hpp:95-145)
def c0(s, used, row):
    x, y, w, h = used
    if row: k, L, first, p0 = h, w, y, x
    else: k, L, first, p0 = w, h, x, y
    get = (lambda a, b, lo, hi: s[a:b, lo:hi]) if row else (lambda a, b, lo, hi: s[lo:hi, a:b].T)
    item = get(first, first + k, p0, p0 + L).astype(np.float64)
    bef = get(first - 1, first, p0, p0 + L)[0].astype(np.float64)
    aft = get(first + k, first + k + 1, p0, p0 + L)[0].astype(np.float64)
    dn = item.mean() - 0.5 * (bef.mean() + aft.mean())
    res = bef[1:-1] - 0.5 * (bef[:-2] + bef[2:])
    sp = res.std(ddof=1) / math.sqrt(1.5)
    pred = sp * math.sqrt(1.0 / (k * L) + 0.5 / L)
    return dn, pred


# ------------------------------------------------------------------ C(dx, dy) estimation (3판 R4 + 4판 R4 보충) and C2
def estimate_C(s, mask):
    """s float32 (H,W); mask bool True = excluded (callers pass truth/map rows+cols +-3, defect pixels, extra bands). Non-finite pixels and
    pixels outside the ROI are added here BEFORE the row/column means. Returns (C dict {(dx,dy): value}, min pair count, ok)."""
    m = mask.copy()
    m |= ~np.isfinite(s)
    roi = np.zeros((H, W), bool); roi[Y0:Y1, X0:X1] = True
    m |= ~roi
    v = ~m
    sv = np.where(v, s, 0).astype(np.float64)
    nr, nc = v.sum(1), v.sum(0)
    rmean = np.divide(sv.sum(1), nr, out=np.zeros(H), where=nr > 0)
    cmean = np.divide(sv.sum(0), nc, out=np.zeros(W), where=nc > 0)
    gmean = sv.sum() / v.sum()
    v &= (nr > 0)[:, None] & (nc > 0)[None, :]                      # rows/columns without any unmasked pixel leave e
    e = np.where(v, sv - rmean[:, None] - cmean[None, :] + gmean, 0.0)
    Cd, nmin = {}, None
    for dy in range(0, R + 1):
        for dx in range(-R, R + 1):
            if dy == 0 and dx < 0: continue
            a = slice(0, H - dy), slice(max(0, -dx), W - max(0, dx))
            b = (slice(dy, H), slice(max(0, dx), W + min(0, dx)))
            pv = v[a] & v[b]
            n = int(pv.sum())
            c = float((e[a] * e[b])[pv].sum() / n) if n else float('nan')
            Cd[(dx, dy)] = c; Cd[(-dx, -dy)] = c
            nmin = n if nmin is None else min(nmin, n)
    ok = nmin >= MIN_PAIRS and all(np.isfinite(list(Cd.values())))
    return Cd, nmin, ok


def c2_sigma(Cd, k, L):
    """Var(D_n) = w' Sigma w for D = mean(k item lines) - (before + after)/2 over extent L (2판 F339-1); lag along the lines = dx for a row part.
    The caller transposes the lag axes for a column part. Lines across: before at -1, items 0..k-1, after at k."""
    lines = [(-1, -0.5 / L)] + [(t, 1.0 / (k * L)) for t in range(k)] + [(k, -0.5 / L)]
    var = 0.0
    for a, ca in lines:
        for b, cb in lines:
            dyv = b - a
            if abs(dyv) > R: continue
            var += ca * cb * sum((L - abs(dxv)) * Cd[(dxv, dyv)] for dxv in range(-R, R + 1) if abs(dxv) < L)
    return math.sqrt(var) if var > 0 and math.isfinite(var) else None


def c2_for(Cd, used, row):
    k, L = (used[3], used[2]) if row else (used[2], used[3])
    if row: return c2_sigma(Cd, k, L)
    Ct = {(dx, dy): Cd[(dy, dx)] for (dx, dy) in Cd}                 # column part: along = y, across = x
    return c2_sigma(Ct, k, L)


# ------------------------------------------------------------------ pipeline (shipped xpe_preprocess.dll, kCfgFinal, same loads as loadSet: xpe_bpm_eval.cpp:2346-2373)
class Buf(C.Structure):
    _fields_ = [("width", C.c_uint32), ("height", C.c_uint32), ("bitsAllocated", C.c_uint32), ("bitsStored", C.c_uint32), ("format", C.c_int), ("data", C.c_void_p), ("dataSize", C.c_size_t)]


class Meta(C.Structure):
    _fields_ = [("bodyPart", C.c_char * 64), ("kVp", C.c_float), ("mAs", C.c_float), ("SID_mm", C.c_float), ("pixelPitch_mm", C.c_float), ("acquisitionTime", C.c_uint64), ("flags", C.c_uint32)]


def pipeline(setdir, frames):
    os.add_dll_directory(BIN)
    pre = C.CDLL(BIN + '/xpe_preprocess.dll'); com = C.CDLL(BIN + '/xpe_common.dll')
    pre.xpe_preprocess_init.argtypes = [C.c_char_p]
    assert pre.xpe_preprocess_init(None) == 0
    for fn in ('xpe_calib_load_offset', 'xpe_calib_load_gain', 'xpe_calib_load_defect_map'): getattr(pre, fn).argtypes = [C.c_char_p]
    com.xpe_clear_alerts()
    rc = [pre.xpe_calib_load_offset(f'{setdir}/offset.xcal'.encode()), pre.xpe_calib_load_gain(f'{setdir}/gain.xcal'.encode()), pre.xpe_calib_load_defect_map(f'{setdir}/defect.xcal'.encode())]
    assert rc == [0, 0, 0], rc
    pre.xpe_preprocess_pipeline_out.argtypes = [C.POINTER(Buf), C.POINTER(Buf), C.POINTER(Meta), C.c_void_p, C.c_void_p, C.c_char_p]
    outs = []
    for f in frames:
        raw = np.fromfile(f, np.uint16); assert raw.size == W * H
        out = np.zeros(W * H, np.float32)
        bi = Buf(W, H, 16, 16, 0, raw.ctypes.data, raw.nbytes); bo = Buf(W, H, 32, 32, 1, out.ctypes.data, out.nbytes); m = Meta()
        r = pre.xpe_preprocess_pipeline_out(C.byref(bi), C.byref(bo), C.byref(m), None, None, CFG_FINAL)
        com.xpe_clear_alerts()
        assert r == 0, r
        outs.append(out.reshape(H, W))
    return outs


def read_xcal_payload(path, dtype):
    b = open(path, 'rb').read()
    cfg = int.from_bytes(b[104:112], 'little'); pl = int.from_bytes(b[112:120], 'little')   # XCalFileHeader (xcal_format.h:129-142, pack 1): 152 bytes, then config, then payload
    assert b[:4] == b'XCAL'
    return np.frombuffer(b[152 + cfg:152 + cfg + pl], dtype)


def s_ref_base(s):
    """std(Dark_last - Dark_second_last)/sqrt2 over ROI pixels, divided by the gain-map median (6-2: the last two calibration darks in file order).
    Returns (base, reason): s_ref(k,L) = base / sqrt(k L). reason != None -> injection impossible (5판 §2)."""
    d1 = np.fromfile(f"{s['folder']}/{s['dark'][-1][0]}", np.uint16).reshape(H, W).astype(np.float64)
    d0 = np.fromfile(f"{s['folder']}/{s['dark'][-2][0]}", np.uint16).reshape(H, W).astype(np.float64)
    diff = (d1 - d0)[Y0:Y1, X0:X1]
    g = read_xcal_payload(f"{TMP}/{s['set']}/gain.xcal", np.float32)
    assert g.size == W * H
    gmed = float(np.median(g))
    sd = float(diff.std()) / math.sqrt(2)
    if not np.any(diff): return None, 'duplicate darks', (sd, gmed)
    if not (math.isfinite(gmed) and gmed > 0): return None, 'g_med', (sd, gmed)
    base = sd / gmed
    if not (math.isfinite(base) and base > 0): return None, 's_ref', (sd, gmed)
    return base, None, (sd, gmed)


# ------------------------------------------------------------------ 7-4 pre-check (masks + geometry only)
def cmd_geom(locked, outdir):
    P = parts_table()
    with open(f'{outdir}/11_parts_table.tsv.txt', 'w') as f:
        f.write('idx\tid\tpart\torig_rect\tdirection\tk\tstatus\tused_rect\tL\n')
        for p in P:
            f.write(f"{p['idx']}\t{p['id']}\t{p['part']}\t{','.join(map(str, p['rect']))}\t{'row' if p['row'] else 'column'}\t{p['k']}\t{p['status']}\t"
                    f"{','.join(map(str, p['used'])) if p['used'] else '-'}\t{p['L'] if p['L'] else '-'}\n")
    st = {}
    for p in P: st[p['status']] = st.get(p['status'], 0) + 1
    n0 = [p for p in P if p['used']]
    print(f"parts {len(P)} status {st} n0 {len(n0)} (row {sum(p['row'] for p in n0)}, column {sum(not p['row'] for p in n0)})")
    comp = {}
    for p in n0: comp[(('row' if p['row'] else 'column'), p['k'], p['L'])] = comp.get((('row' if p['row'] else 'column'), p['k'], p['L']), 0) + 1
    print('composition of the n0 used parts (direction, k, L): count ->', dict(sorted(comp.items())))
    rows = []
    for s in json.load(open(locked, encoding='utf-8')):
        rs_t, cs_t, rs_m, cs_m, n = line_masks(f"{TMP}/{s['set']}/lines.tsv")
        cnt = [len(admissible(p, rs_t | rs_m, cs_t | cs_m)) for p in n0]
        imp = sum(c == 0 for c in cnt)
        rows.append((s['side'], s['set'], n['line'], n['candidate'], int(rs_m.sum()), int(cs_m.sum()), imp, len(n0) - imp, min(cnt), int(np.median(cnt)), max(cnt), cnt))
        print(f"{s['side']}\t{s['set']}\tmap line {n['line']}\tcandidate {n['candidate']}\tmasked map rows {int(rs_m.sum())} cols {int(cs_m.sum())}\t"
              f"placement-impossible {imp}\tn_eff {len(n0) - imp} (both frames)\tadmissible per part min/median/max {min(cnt)}/{int(np.median(cnt))}/{max(cnt)}", flush=True)
    with open(f'{outdir}/13_admissible_per_part.tsv.txt', 'w') as f:
        f.write('set\t' + '\t'.join(f"p{p['idx']}" for p in n0) + '\n')
        for r in rows: f.write(r[1] + '\t' + '\t'.join(map(str, r[-1])) + '\n')
    print('frames with n_eff 0:', sum(2 for r in rows if r[7] == 0), 'of', 2 * len(rows))



# ------------------------------------------------------------------ per-frame measurement
def base_mask(setname):
    rs_t, cs_t, rs_m, cs_m, n = line_masks(f"{TMP}/{setname}/lines.tsv")
    m = np.fromfile(f"{TMP}/{setname}/map.u8", np.uint8).reshape(H, W) != 0       # defect-map pixels
    m[rs_t | rs_m, :] = True
    m[:, cs_t | cs_m] = True
    return m, rs_t | rs_m, cs_t | cs_m


def band(used, row):
    """the injected part +-3 in row/column units (same reading B as the other line masks)"""
    x, y, w, h = used
    m = np.zeros((H, W), bool)
    if row: m[max(0, y - 3):y + h + 3, :] = True
    else: m[:, max(0, x - 3):x + w + 3] = True
    return m


_G = {}
def _winit(spath, mpath):
    _G['s'] = np.load(spath); _G['m'] = np.load(mpath)
def _wC(arg):
    used, row = arg
    Cd, nmin, ok = estimate_C(_G['s'], _G['m'] | band(used, row))
    return Cd, nmin, ok


def overlap(a, b):
    return a[0] < b[0] + b[2] and b[0] < a[0] + a[2] and a[1] < b[1] + b[3] and b[1] < a[1] + a[3]


def measure_frame(S, bm, rs, cs, n0, rng, sref, pool_paths, draws=20, inject=True):
    from multiprocessing import Pool
    Cb, nminb, okb = estimate_C(S, bm)
    adm = [admissible(p, rs, cs) for p in n0]
    placeable = [i for i, a in enumerate(adm) if a]
    fr = dict(C_ok=okb, C_min_pairs=nminb, C=[Cb[(dx, dy)] for dy in range(R + 1) for dx in range(R + 1)],
              placement_impossible=len(n0) - len(placeable), denominator=len(placeable), draws=[], inj=None)
    first_pick = {}
    ratios_c2 = []
    for t in range(draws):
        pick = {i: int(rng.integers(len(adm[i]))) for i in placeable}
        if t == 0: first_pick = dict(pick)
        o0 = o2 = 0; v0 = v2 = True; rects = []
        for i in placeable:
            u = adm[i][pick[i]]; rects.append(u)
            dn, sp = c0(S, u, n0[i]['row'])
            if not (math.isfinite(dn) and math.isfinite(sp) and sp > 0): v0 = False
            elif abs(dn / sp) > 3: o0 += 1
            s2 = c2_for(Cb, u, n0[i]['row']) if okb else None
            if s2 is None or not math.isfinite(dn): v2 = False
            else:
                o2 += abs(dn / s2) > 3; ratios_c2.append(dn / s2)
        ov = sum(overlap(rects[a], rects[b]) for a in range(len(rects)) for b in range(a + 1, len(rects)))
        fr['draws'].append(dict(c0=o0 if v0 else None, c2=o2 if (v2 and okb) else None, overlap_pairs=ov))
    fr['gt5_c2'] = [int(np.sum(np.abs(ratios_c2) > 5)), len(ratios_c2)]
    if not inject or not placeable: return fr
    np.save(pool_paths[0], S); np.save(pool_paths[1], bm)
    jobs = [(adm[i][first_pick[i]], n0[i]['row']) for i in placeable]
    with Pool(12, _winit, pool_paths) as pool: Cs = pool.map(_wC, jobs, chunksize=1)
    inj = {cand: {a: dict(valid=0, detected=0, already_over=0, undeterminable=0, impossible=0) for a in A_LIST} for cand in ('c0', 'c2')}
    for j, i in enumerate(placeable):
        u, row = jobs[j]; p = n0[i]
        k, L = (u[3], u[2]) if row else (u[2], u[3])
        dn, sp = c0(S, u, row)
        Cd, nmin, ok = Cs[j]
        s2 = c2_for(Cd, u, row) if ok else None
        sign = 1.0 if p['idx'] % 2 == 0 else -1.0
        for cand, sig in (('c0', sp if (math.isfinite(sp) and sp > 0) else None), ('c2', s2)):
            for a in A_LIST:
                r = inj[cand][a]
                if sref is None: r['impossible'] += 1; continue
                if sig is None or not math.isfinite(dn): r['undeterminable'] += 1; continue
                if abs(dn / sig) > 3: r['already_over'] += 1; continue
                A = a * sref / math.sqrt(k * L)
                rr = (dn + sign * A) / sig
                r['valid'] += 1; r['detected'] += int(abs(rr) > 3 and rr * sign > 0)
    fr['inj'] = inj
    return fr


def cmd_run(locked, side, outdir):
    sets = [s for s in json.load(open(locked, encoding='utf-8')) if s['side'] == side]
    n0 = [p for p in parts_table() if p['used']]
    rng = np.random.default_rng(339)            # one stream per side, continued over frames in locked-list order
    res = []
    pp = (f'{TMP}/_S_{side}.npy', f'{TMP}/_M_{side}.npy')
    for s in sets:
        bm, rs, cs = base_mask(s['set'])
        sref, why, (sd, gmed) = s_ref_base(s)
        outs = pipeline(f"{TMP}/{s['set']}", [f"{s['folder']}/{n}" for n, _ in s['ev']])
        for (fname, _), S in zip(s['ev'], outs):
            fr = measure_frame(S, bm, rs, cs, n0, rng, sref, pp)
            fr.update(set=s['set'], panel=s['panel'], frame=fname, s_ref_base=sref, s_ref_reason=why, dark_sd_over_sqrt2=sd, g_med=gmed,
                      out_sha256=__import__('hashlib').sha256(S.tobytes()).hexdigest())
            res.append(fr)
            print(json.dumps({k: fr[k] for k in ('set', 'frame', 'C_ok', 'C_min_pairs', 'denominator', 'placement_impossible')}), flush=True)
            json.dump(res, open(f'{outdir}/{side}_results.json.txt', 'w'), indent=0)
    for f in pp:
        if os.path.exists(f): os.remove(f)



# ------------------------------------------------------------------ equivalence checks (cyan_test Bright_29)
def cmd_check(locked):
    import time
    s = [x for x in json.load(open(locked, encoding='utf-8')) if x['set'] == 'cyan_test'][0]
    S = pipeline(f"{TMP}/cyan_test", [f"{s['folder']}/Bright_29.raw"])[0]
    # (a) the C0 port + this pipeline call against xpe_bpm_eval correct (QA-A-331, no margins, ratio_clean of frame 1)
    ref = json.load(open('D:/workspace-github/xpe-pre/.moai/reports/lane-pre/QA-A-331/evidence/valid_correct_stage.json.txt'))['frames'][0]['line_index']
    P = parts_table(); dmax = 0.0; over = 0
    for p, r in zip(P, ref):
        assert (p['id'], p['part']) == (r['id'], r['part'])
        dn, sp = c0(S, p['rect'], p['row']); q = dn / sp; over += abs(q) > 3
        dmax = max(dmax, abs(round(q, 4) - r['ratio_clean']))
    print(f"(a) C0 port vs xpe_bpm_eval QA-A-331 ratio_clean, 142 parts in the same order, full-frame rects: max |diff| {dmax:.6f} (json has 4 decimals); clean over-3 count {over} (QA-A-331: 8)")
    # (b) injection shortcut: ratio = (D0 + sign A)/sigma with C re-estimated on the CLEAN frame with the band masked, vs literally adding A to the item pixels
    bm, rs, cs = base_mask('cyan_test'); n0 = [p for p in P if p['used']]
    t = time.time(); Cb, _, _ = estimate_C(S, bm); print(f"estimate_C seconds {time.time() - t:.2f}")
    for p in (n0[0], n0[1], n0[5]):
        u = admissible(p, rs, cs)[0]; row = p['row']; x, y, w, h = u; k, L = (h, w) if row else (w, h)
        dn, sp = c0(S, u, row); Cd, _, _ = estimate_C(S, bm | band(u, row)); s2 = c2_for(Cd, u, row)
        for A in (7.5, -3.25):
            T = S.copy(); T[y:y + h, x:x + w] += np.float32(A)
            dn2, sp2 = c0(T, u, row); Cd2, _, _ = estimate_C(T, bm | band(u, row)); s22 = c2_for(Cd2, u, row)
            print(f"(b) part idx {p['idx']} used {u} A {A}: C0 shortcut {(dn + A) / sp:.9f} literal {dn2 / sp2:.9f} | C2 shortcut {(dn + A) / s2:.9f} literal {dn2 / s22:.9f} | C identical {all(Cd[q] == Cd2[q] for q in Cd)}")



# ------------------------------------------------------------------ summary and contamination control
def nearest_rank(v, q=0.99):
    v = sorted(v)
    return v[math.ceil(q * len(v)) - 1] if v else None


def summarize(res, title):
    out = [f'## {title}', f"frames {len(res)} | sets {len(set(r['set'] for r in res))} | C2-undeterminable frames {sum(not r['C_ok'] for r in res)}",
           f"n_eff per frame (n0 - placement impossible) distribution: { {v: sum(r['denominator'] == v for r in res) for v in sorted(set(r['denominator'] for r in res))} } | placement-impossible per frame: {sorted(set(r['placement_impossible'] for r in res))}"]
    for cand in ('c0', 'c2'):
        # 9판: the null statistic is the exceedance ratio (#|ratio|>3) / n_eff per draw; the percentile is taken on the ratio
        vals = [d[cand] / r['denominator'] for r in res for d in r['draws'] if d[cand] is not None and r['denominator'] > 0]
        und = sum(d[cand] is None for r in res for d in r['draws'])
        hist = {}
        for v in vals: hist[v] = hist.get(v, 0) + 1
        out.append(f"{cand}: valid draws M {len(vals)}, undeterminable draws {und}, nearest-rank 99th percentile of the exceedance ratio {round(nearest_rank(vals), 5) if vals else 'no criterion (M=0)'}, "
                   f"mean {np.mean(vals) if vals else float('nan'):.5f}, distinct ratio values (value: draws) {dict((round(k, 5), v) for k, v in sorted(hist.items()))}")
    ov = [d['overlap_pairs'] for r in res for d in r['draws']]
    out.append(f"overlapping moved-part pairs per draw: min {min(ov)} median {int(np.median(ov))} max {max(ov)}")
    g = [r['gt5_c2'] for r in res]
    out.append(f"|D|/sigma_C2 > 5 among null-draw parts (diagnostic only): {sum(a for a, _ in g)} / {sum(b for _, b in g)}")
    for cand in ('c0', 'c2'):
        for a in A_LIST:
            t = {k: sum(r['inj'][cand][str(a)][k] if isinstance(next(iter(r['inj'][cand])), str) else r['inj'][cand][a][k] for r in res if r['inj']) for k in ('valid', 'detected', 'already_over', 'undeterminable', 'impossible')}
            out.append(f"injection {cand} a={a}: detected {t['detected']} / valid {t['valid']} = {t['detected'] / t['valid'] if t['valid'] else float('nan'):.4f} | already over before injection {t['already_over']}, undeterminable {t['undeterminable']}, injection impossible {t['impossible']}")
    return '\n'.join(out)


def cmd_summary(outdir):
    txt = []
    for side in ('dev', 'val'):
        f = f'{outdir}/{side}_results.json.txt'
        if not os.path.exists(f): continue
        res = json.load(open(f))
        txt.append(summarize(res, side))
        for r in res:
            d0 = [d['c0'] for d in r['draws']]; d2 = [d['c2'] for d in r['draws']]
            txt.append(f"  {r['set']} {r['frame']}: C_ok {r['C_ok']} min pairs {r['C_min_pairs']} denominator {r['denominator']} s_ref_base {r['s_ref_base']} ({r['s_ref_reason']}) "
                       f"C(0,0) {r['C'][0]:.4g} | c0 counts {d0} | c2 counts {d2}")
    print('\n'.join(txt))


def cmd_contam(locked, outdir):
    s = [x for x in json.load(open(locked, encoding='utf-8')) if x['set'] == 'cyan_test'][0]
    S = pipeline(f"{TMP}/cyan_test", [f"{s['folder']}/Bright_29.raw"])[0]
    bm, rs, cs = base_mask('cyan_test'); n0 = [p for p in parts_table() if p['used']]
    sref, why, _ = s_ref_base(s)
    Cb, _, ok = estimate_C(S, bm)
    sig = c2_sigma(Cb, 1, X1 - X0)                                   # sigma_C2(k=1, L=ROI width) of the original output
    rng = np.random.default_rng(339)
    rows = np.arange(Y0, (Y0 + Y1) // 2)                             # ROI rows above the vertical centre line
    T = S.copy(); T[rows, X0:X1] += rng.normal(0.0, 2 * sig, rows.size).astype(np.float32)[:, None]
    res = {}
    for name, img in (('before', S), ('after', T)):
        fr = measure_frame(img, bm, rs, cs, n0, np.random.default_rng(339), sref, (f'{TMP}/_S_c.npy', f'{TMP}/_M_c.npy'))
        fr.update(set='cyan_test', frame='Bright_29 ' + name)
        res[name] = fr
    json.dump(dict(sigma_ref=sig, rows=[int(rows[0]), int(rows[-1])], res=res), open(f'{outdir}/contam_results.json.txt', 'w'), indent=0)
    print(f"sigma_ref (sigma_C2, k=1, L={X1 - X0}) = {sig:.6g}; offset rows {rows[0]}..{rows[-1]} ({rows.size}), N(0, (2 sigma_ref)^2), seed 339")
    for name in ('before', 'after'):
        r = res[name]
        print(f"[{name}] C(dx,dy) dy=0..3 rows, dx=0..3 cols: {np.round(np.array(r['C']).reshape(4, 4), 4).tolist()} min pairs {r['C_min_pairs']}")
        print(summarize([r], name))


if __name__ == '__main__':
    cmd = sys.argv[1]
    if cmd == 'geom': cmd_geom(sys.argv[2], sys.argv[3])
    if cmd == 'check': cmd_check(sys.argv[2])
    if cmd == 'summary': cmd_summary(sys.argv[2])
    if cmd == 'contam': cmd_contam(sys.argv[2], sys.argv[3])
    if cmd == 'run': cmd_run(sys.argv[2], sys.argv[3], sys.argv[4])
