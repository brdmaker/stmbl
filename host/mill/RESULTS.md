# mill_sim sample runs (python3 runs.py --md RESULTS.md)

The Scripta CN 720 model (machine/cn720*.mill). Values in um unless named
otherwise: econt the tool's distance from the commanded XY path, etool
from the commanded point, ez in Z, yb the bridge's sway, hx the head's
swing in x, ferr command - feedback.


**Standing still, 1 s**

| variant | rms_etool | max_etool | rms_ez | max_ez |
|---|---|---|---|---|
| resolver 1k | 1.31 | 1.39 | 0.523 | 0.566 |
| resolver 2k | 0.434 | 0.538 | 0.828 | 0.87 |
| scales 1k | 3.63 | 3.68 | 0.32 | 0.332 |
| scales 2k | 3.04 | 3.08 | 0.0192 | 0.0338 |
| blend 1k | 0.842 | 0.925 | 0.506 | 0.547 |
| blend 2k | 0.36 | 0.44 | 0.202 | 0.242 |

**Circle R 10 mm at 3 m/min (0.03 g), 2nd and 3rd turn**

| variant | max_econt | rms_econt | max_yb | max_hx |
|---|---|---|---|---|
| resolver 1k | 4.8 | 1.4 | 0.655 | 2.28 |
| resolver 2k | 4.76 | 0.932 | 0.694 | 2.36 |
| scales 1k | 8.38 | 3.97 | 1.17 | 2.7 |
| scales 2k | 8.89 | 4.01 | 1.12 | 3.14 |
| blend 1k | 7.63 | 3.67 | 0.587 | 2.26 |
| blend 2k | 6.92 | 3.67 | 0.679 | 2.39 |

**Circle R 10 mm at 6 m/min (0.1 g)**

| variant | max_econt | rms_econt | max_yb | max_hx |
|---|---|---|---|---|
| resolver 1k | 12.1 | 1.83 | 1.77 | 5.94 |
| resolver 2k | 8.62 | 1.46 | 1.8 | 5.19 |
| scales 1k | 14.8 | 3.95 | 1.79 | 6.18 |
| scales 2k | 9.89 | 3.96 | 2.02 | 5.73 |
| blend 1k | 14.5 | 3.75 | 1.81 | 5.99 |
| blend 2k | 10.3 | 3.75 | 1.78 | 5.16 |

**Circle R 10 mm at 12 m/min (0.4 g)**

| variant | max_econt | rms_econt | max_yb | max_hx |
|---|---|---|---|---|
| resolver 1k | 16 | 4.89 | 8.34 | 12.4 |
| resolver 2k | 10.8 | 3.41 | 7.44 | 11.1 |
| scales 1k | 18.7 | 6.04 | 8.45 | 11 |
| scales 2k | 11.8 | 5.25 | 7.62 | 10 |
| blend 1k | 16.7 | 5.86 | 8.39 | 12.3 |
| blend 2k | 14.9 | 4.76 | 7.42 | 11.4 |

**Circle R 2 mm at 3 m/min (0.35 g): a small pocket**

| variant | max_econt | rms_econt | max_yb | max_hx |
|---|---|---|---|---|
| resolver 1k | 13.3 | 2.27 | 2.35 | 7.69 |
| resolver 2k | 9.62 | 1.9 | 2.43 | 6.31 |
| scales 1k | 19.3 | 4.43 | 2.45 | 8.35 |
| scales 2k | 13.8 | 4.26 | 2.62 | 6.81 |
| blend 1k | 16.9 | 3.9 | 2.4 | 7.83 |
| blend 2k | 11.2 | 3.74 | 2.5 | 6.19 |

**20 mm square, exact stop at 1 g per side (15.5 m/min peak)**

| variant | max_econt | rms_econt | max_yb | max_hx |
|---|---|---|---|---|
| resolver 1k | 35.9 | 8.05 | 29.2 | 52.5 |
| resolver 2k | 39.6 | 8.2 | 25.8 | 47.4 |
| scales 1k | 32.7 | 9.16 | 29.6 | 53 |
| scales 2k | 34.3 | 9.16 | 26.2 | 47.8 |
| blend 1k | 33.5 | 8.26 | 29.2 | 52.5 |
| blend 2k | 36.2 | 8.31 | 25.7 | 47.3 |

**20 mm square, exact stop at 0.5 g (11 m/min peak)**

| variant | max_econt | rms_econt | max_yb | max_hx |
|---|---|---|---|---|
| resolver 1k | 22.5 | 4.61 | 21.5 | 27.8 |
| resolver 2k | 22.6 | 4.59 | 20.3 | 24.1 |
| scales 1k | 28.1 | 6.45 | 21.6 | 28 |
| scales 2k | 28.2 | 6.6 | 20.2 | 24.6 |
| blend 1k | 24.4 | 5.18 | 21.6 | 27.8 |
| blend 2k | 25 | 5.15 | 20.3 | 24.2 |

**20 mm square, exact stop, S-curve sides in the same time (1.1 g peak, jerk-limited)**

| variant | max_econt | rms_econt | max_yb | max_hx |
|---|---|---|---|---|
| resolver 1k | 19.6 | 4.68 | 37.9 | 22.6 |
| resolver 2k | 17.8 | 4.46 | 37.3 | 22.8 |
| scales 1k | 21.6 | 6.33 | 37.9 | 22.7 |
| scales 2k | 21.2 | 6.25 | 37.3 | 22.6 |
| blend 1k | 17.8 | 4.98 | 37.9 | 22.5 |
| blend 2k | 17 | 4.81 | 37.3 | 22.8 |

**X line 300 mm at 3 m/min (feed), 0.25 g ramps**

| variant | rms_etool | rms_econt | max_econt | rms_ferr_x |
|---|---|---|---|---|
| resolver 1k | 20.4 | 0.915 | 1.05 | 1.21 |
| resolver 2k | 2.99 | 0.294 | 0.427 | 2.71 |
| scales 1k | 23 | 3.52 | 3.65 | 5.44 |
| scales 2k | 3.73 | 2.89 | 3.05 | 5.88 |
| blend 1k | 22.7 | 0.729 | 0.875 | 5.65 |
| blend 2k | 1.91 | 0.141 | 0.347 | 6.2 |

**Y rapid 300 mm at 27 m/min, 1 g ramps (0.71 s), then where the tool is after 10 / 30 / 100 ms**

| variant | etool@0 | etool@10ms | etool@30ms | etool@100ms | max_yb | max_econt |
|---|---|---|---|---|---|---|
| resolver 1k | 1.38 | 17.2 | 2.09 | 6.56 | 37.3 | 24.6 |
| resolver 2k | 1.5 | 21 | 1.61 | 7.45 | 33.7 | 23.7 |
| scales 1k | 5.83 | 20.8 | 3.53 | 3.39 | 37.8 | 28.8 |
| scales 2k | 5.71 | 27 | 5.71 | 3.23 | 34 | 27.7 |
| blend 1k | 2.38 | 16.3 | 3.14 | 3.42 | 37.5 | 26.1 |
| blend 2k | 3.36 | 19.6 | 3.53 | 3.94 | 33.6 | 25.5 |

**X 600 mm with a 5 K warm screw (57 um/m too long), the error at the end**

| variant | etool | tx | cx |
|---|---|---|---|
| resolver 1k | 38.3 | 650 | 650 |
| resolver 2k | 37.1 | 650 | 650 |
| scales 1k | 5.21 | 650 | 650 |
| scales 2k | 4.91 | 650 | 650 |
| blend 1k | 4.67 | 650 | 650 |
| blend 2k | 4.44 | 650 | 650 |

**Steady 20 N cutting force on the tool in X and Y (thin chips)**

| variant | etool | max_hx | max_hy |
|---|---|---|---|
| resolver 1k | 16.5 | 7.46 | 10.8 |
| resolver 2k | 15.3 | 7.46 | 10.8 |
| scales 1k | 23.2 | 7.46 | 10.8 |
| scales 2k | 23.4 | 7.46 | 10.8 |
| blend 1k | 16.8 | 7.46 | 10.8 |
| blend 2k | 16.1 | 7.46 | 10.8 |
