# QA-A-340 control for the defect-map line reading (leader 07:34): feed files with KNOWN line records to the SAME function
# line_masks() of 10_study.py. Positive control: QA-A-313 51_lines_v2 (planted set, 76 line rows). Negative control: QA-A-313 52_lines_clean_v2 (clean set, header only).
import sys, importlib.util
spec = importlib.util.spec_from_file_location("study", "10_study.py"); m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m)
for f in ("../../QA-A-313/evidence/51_lines_v2.tsv.txt", "../../QA-A-313/evidence/52_lines_clean_v2.tsv.txt"):
    rs_t, cs_t, rs_m, cs_m, n = m.line_masks(f)
    data = [l.split("\t")[0] for l in open(f).read().splitlines()[1:]]
    print(f, "| file records by kind:", {k: data.count(k) for k in sorted(set(data))}, "| line_masks counts:", n, "| masked map rows", int(rs_m.sum()), "cols", int(cs_m.sum()))
