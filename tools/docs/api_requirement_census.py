"""#211: exported preprocess API vs REQ-P1A requirements that name it.

SCOPE LIMIT -- read before believing a "no requirement" row.

This scans SPEC-XPE-P1A ONLY. A function whose requirement lives in another
series is reported as uncovered even though it is not. That has already bitten
twice (2026-09-28):

  xpe_calib_get_quality_meta  -> SRS-CALIB-FUNC-033 (#140, dispositioned)
  xpe_nonlinearity_correct    -> SRS-CALIB-FUNC-006 + -006-EXT (#186)

Both are real requirements in the SRS-CALIB series. Treat an orphan row as
"no REQ-P1A requirement names this", never as "this function has no
requirement" -- confirm against SRS-CALIB-001 before acting.

An API is COVERED when some requirement's body names the function. The control
is deliberate: run the same match against a function that is known to be named
in a requirement (xpe_defect_correct, REQ-P1A-012) and against one that cannot
be (a fabricated name). If the control fails, the matcher is blind and the
absence counts mean nothing.
"""
import io, re, pathlib

REPO = pathlib.Path(r"D:/workspace-github/image-processing")
HDR = REPO / "modules/preprocess/include/xpe/preprocess_api.h"
SPEC = REPO / ".moai/specs/SPEC-XPE-P1A/spec.md"

hdr = io.open(HDR, encoding="utf-8", errors="replace").read()
spec = io.open(SPEC, encoding="utf-8", errors="replace").read()

apis = sorted(set(re.findall(r"^XPE_API\s+\S+\s+(xpe_[a-z0-9_]+)", hdr, re.M)))

# Split the spec into requirement blocks so we can say WHICH req names an API.
blocks = {}
cur = None
# A block ends at the NEXT HEADING OF ANY LEVEL, not at the next REQ heading.
# Measured bug (#211, 2026-09-28): with the looser boundary, REQ-P1A-091's block
# swallowed the prose of the following section and the control
# (xpe_defect_correct) matched it. Coverage was over-counted; orphans are a
# LOWER bound until this holds.
for line in spec.splitlines():
    m = re.match(r"^#{2,4}\s+(REQ-P1A-[0-9]{3}[a-z]?):", line)
    if m:
        cur = m.group(1)
        blocks[cur] = []
    elif re.match(r"^#{1,6}\s", line):
        cur = None
    elif cur:
        blocks[cur].append(line)
bodies = {k: "\n".join(v) for k, v in blocks.items()}

def named_by(fn):
    return sorted(r for r, b in bodies.items() if fn in b)

# --- controls, printed first: if these are wrong, ignore everything below
ctl_pos = named_by("xpe_defect_correct")
ctl_neg = named_by("xpe_this_function_does_not_exist")
print(f"CONTROL positive xpe_defect_correct -> {ctl_pos or 'NONE (matcher blind!)'}")
print(f"CONTROL negative fabricated name    -> {ctl_neg or 'NONE (correct)'}")
print(f"requirement blocks parsed: {len(bodies)}")
print(f"exported APIs: {len(apis)}")
print()

covered, orphan = [], []
for fn in apis:
    reqs = named_by(fn)
    (covered if reqs else orphan).append((fn, reqs))

print(f"=== NAMED BY A REQUIREMENT: {len(covered)}")
for fn, reqs in covered:
    print(f"  {fn:38s} {','.join(reqs)}")
print()
print(f"=== NAMED BY NO REQUIREMENT: {len(orphan)}")
for fn, _ in orphan:
    print(f"  {fn}")
