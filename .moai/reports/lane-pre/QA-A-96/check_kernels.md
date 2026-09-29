## 1. fit residual RMS by radius band (median / max over 42 cases)

| r (cm) | gauss2 median | gauss2 max | gauss4 median | gauss4 max | PSF seed SEM (median) |
|---|---|---|---|---|---|
| 0-0.5 | 44.7 % | 65.7 % | 40.2 % | 73.2 % | 0.40 % |
| 0.5-2 | 26.9 % | 42.2 % | 4.1 % | 12.9 % | 0.27 % |
| 2-5 | 24.6 % | 40.8 % | 2.0 % | 6.6 % | 0.21 % |
| 5-10 | 15.0 % | 33.2 % | 1.3 % | 4.3 % | 0.10 % |
| 10-15 | 14.0 % | 26.0 % | 1.0 % | 3.5 % | 0.14 % |
| 15-30 | 12.5 % | 24.6 % | 1.1 % | 3.2 % | 0.30 % |

gauss2 fit_rms: min 0.104 median 0.167 max 0.303 | tail_rms: min 0.077 median 0.125 max 0.246

gauss4 fit_rms: min 0.030 median 0.054 max 0.105 | tail_rms: min 0.006 median 0.011 max 0.032

## 2. SPR (30 x 30, csi) monotonicity

| thickness \ kVp | 60 | 70 | 80 | 90 | 100 | 110 | 120 |
|---|---|---|---|---|---|---|---|
| 5 | 0.626 | 0.628 | 0.631 | 0.633 | 0.636 | 0.639 | 0.641 |
| 10 | 1.350 | 1.373 | 1.396 | 1.414 | 1.431 | 1.446 | 1.460 |
| 15 | 2.178 | 2.234 | 2.284 | 2.330 | 2.369 | 2.403 | 2.437 |
| 20 | 3.106 | 3.206 | 3.294 | 3.374 | 3.443 | 3.505 | 3.565 |
| 25 | 4.136 | 4.283 | 4.422 | 4.542 | 4.656 | 4.759 | 4.850 |
| 30 | 5.262 | 5.477 | 5.662 | 5.825 | 6.005 | 6.148 | 6.297 |

non-increasing steps along thickness: 0 []
non-increasing steps along kVp: 0 []
SPR relative SEM: max 0.2840 %

## 3a. coefficient smoothness (|value / arithmetic mean of the two neighbours - 1|, interior points)

### gauss2

| param | along thickness: median / max (at) | along kVp: median / max (at) |
|---|---|---|
| a1 | 5.0 % / 7.8 % (10 cm, 120 kVp) | 0.2 % / 1.7 % (25 cm, 70 kVp) |
| a2 | 14.3 % / 28.1 % (10 cm, 60 kVp) | 0.7 % / 1.3 % (25 cm, 70 kVp) |
| s1 | 1.7 % / 3.8 % (10 cm, 120 kVp) | 0.1 % / 0.7 % (30 cm, 100 kVp) |
| s2 | 0.3 % / 1.4 % (10 cm, 60 kVp) | 0.1 % / 0.3 % (30 cm, 90 kVp) |

### gauss4

| param | along thickness: median / max (at) | along kVp: median / max (at) |
|---|---|---|
| a1 | 4.5 % / 10.9 % (25 cm, 70 kVp) | 1.7 % / 5.6 % (30 cm, 80 kVp) |
| a2 | 4.0 % / 13.3 % (10 cm, 90 kVp) | 0.7 % / 4.7 % (25 cm, 80 kVp) |
| a3 | 2.5 % / 11.4 % (10 cm, 60 kVp) | 0.4 % / 2.0 % (30 cm, 70 kVp) |
| a4 | 19.6 % / 33.9 % (10 cm, 90 kVp) | 1.0 % / 2.4 % (25 cm, 80 kVp) |
| s1 | 1.1 % / 5.1 % (25 cm, 70 kVp) | 0.7 % / 3.3 % (30 cm, 70 kVp) |
| s2 | 1.8 % / 3.6 % (10 cm, 90 kVp) | 0.4 % / 2.8 % (30 cm, 90 kVp) |
| s3 | 0.7 % / 1.9 % (20 cm, 60 kVp) | 0.3 % / 1.5 % (30 cm, 90 kVp) |
| s4 | 0.5 % / 2.2 % (10 cm, 60 kVp) | 0.1 % / 0.8 % (30 cm, 90 kVp) |


## 3b. coefficient smoothness (|value / geometric mean of the two neighbours - 1|, interior points)

### gauss2

| param | along thickness: median / max (at) | along kVp: median / max (at) |
|---|---|---|
| a1 | 7.9 % / 20.1 % (10 cm, 120 kVp) | 0.2 % / 1.6 % (25 cm, 70 kVp) |
| a2 | 12.9 % / 27.5 % (10 cm, 110 kVp) | 0.8 % / 1.9 % (25 cm, 70 kVp) |
| s1 | 1.7 % / 4.1 % (10 cm, 120 kVp) | 0.1 % / 0.7 % (30 cm, 100 kVp) |
| s2 | 0.2 % / 1.4 % (10 cm, 60 kVp) | 0.1 % / 0.3 % (30 cm, 90 kVp) |

### gauss4

| param | along thickness: median / max (at) | along kVp: median / max (at) |
|---|---|---|
| a1 | 5.1 % / 11.2 % (25 cm, 70 kVp) | 1.3 % / 5.2 % (30 cm, 80 kVp) |
| a2 | 4.7 % / 20.0 % (10 cm, 70 kVp) | 0.7 % / 4.6 % (25 cm, 80 kVp) |
| a3 | 12.0 % / 29.3 % (10 cm, 110 kVp) | 0.5 % / 2.0 % (30 cm, 70 kVp) |
| a4 | 12.1 % / 30.2 % (10 cm, 110 kVp) | 1.8 % / 3.7 % (5 cm, 70 kVp) |
| s1 | 1.3 % / 5.1 % (25 cm, 70 kVp) | 0.6 % / 3.0 % (30 cm, 70 kVp) |
| s2 | 1.8 % / 3.9 % (10 cm, 70 kVp) | 0.4 % / 2.8 % (30 cm, 90 kVp) |
| s3 | 0.7 % / 2.0 % (20 cm, 60 kVp) | 0.3 % / 1.5 % (30 cm, 90 kVp) |
| s4 | 0.5 % / 2.2 % (10 cm, 60 kVp) | 0.1 % / 0.8 % (30 cm, 90 kVp) |

## 4. SPR 30x30 from the fitted kernel vs direct pixel sum

gauss2: median +2.36 %, min -0.58 %, max +5.88 %
gauss4: median -0.40 %, min -1.58 %, max -0.21 %
sum a_i / spr_30x30 (gauss4): min 1.008 max 1.351
