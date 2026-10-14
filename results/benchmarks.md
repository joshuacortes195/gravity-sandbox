# Brute force against Barnes-Hut

Written by `tools/benchmark.cpp`. Same numbers as `benchmarks.json`.

- Machine: Intel Core i5-8259U @ 2.30GHz, 4 physical / 8 logical cores, 8 GB RAM, macOS 15.7.7
- Commit: 84ad469
- Compiler: Apple LLVM 17.0.0 (clang-1700.6.4.2)
- Build flags: Release: -O3 -DNDEBUG
- Softening: 0.01, seed: 1, bodies at rest with equal mass in a ball of radius 1
- Time: one force pass on a single thread, the fastest of at least 3 runs and 0.3 seconds. Tree times include building the tree
- Error: |a_tree - a_brute| / |a_brute| for each body, then the mean and the worst body
- The worst body is measured against its own acceleration. Where the pulls nearly cancel that is small, so the worst reads far higher than the mean

## uniform scene: milliseconds per force pass

| N | brute force | tree build only | theta 0.3 | theta 0.5 | theta 0.7 | theta 1 |
| --- | --- | --- | --- | --- | --- | --- |
| 100 | 0.032 | 0.003 | 0.082 (0.39x) | 0.058 (0.56x) | 0.041 (0.78x) | 0.025 (1.28x) |
| 300 | 0.291 | 0.009 | 0.719 (0.40x) | 0.442 (0.66x) | 0.284 (1.02x) | 0.161 (1.81x) |
| 1000 | 3.367 | 0.050 | 8.121 (0.41x) | 3.396 (0.99x) | 1.768 (1.90x) | 0.852 (3.95x) |
| 3000 | 31.461 | 0.187 | 50.216 (0.63x) | 16.376 (1.92x) | 7.834 (4.02x) | 3.423 (9.19x) |
| 10000 | 369.842 | 0.728 | 258.898 (1.43x) | 78.959 (4.68x) | 33.760 (10.96x) | 14.242 (25.97x) |
| 30000 | 3351.005 | 2.622 | 1117.827 (3.00x) | 313.357 (10.69x) | 135.392 (24.75x) | 53.588 (62.53x) |

## uniform scene: force error, mean (worst body)

| N | theta 0.3 | theta 0.5 | theta 0.7 | theta 1 |
| --- | --- | --- | --- | --- |
| 100 | 1.64e-03 (8.58e-03) | 7.01e-03 (7.69e-02) | 1.93e-02 (8.43e-02) | 5.02e-02 (2.11e-01) |
| 300 | 1.83e-03 (2.44e-02) | 6.97e-03 (7.05e-02) | 2.00e-02 (1.76e-01) | 6.15e-02 (6.09e-01) |
| 1000 | 1.37e-03 (1.44e-02) | 6.46e-03 (4.15e-02) | 1.66e-02 (1.18e-01) | 4.68e-02 (3.65e-01) |
| 3000 | 1.35e-03 (2.00e-02) | 6.77e-03 (9.11e-02) | 1.58e-02 (2.91e-01) | 4.48e-02 (9.15e-01) |
| 10000 | 1.29e-03 (3.43e-02) | 6.12e-03 (1.93e-01) | 1.39e-02 (5.65e-01) | 3.80e-02 (9.67e-01) |
| 30000 | 1.26e-03 (2.53e-02) | 5.85e-03 (8.59e-02) | 1.32e-02 (1.99e-01) | 3.62e-02 (5.55e-01) |

## clustered scene: milliseconds per force pass

| N | brute force | tree build only | theta 0.3 | theta 0.5 | theta 0.7 | theta 1 |
| --- | --- | --- | --- | --- | --- | --- |
| 100 | 0.032 | 0.005 | 0.046 (0.70x) | 0.036 (0.89x) | 0.031 (1.04x) | 0.025 (1.31x) |
| 300 | 0.297 | 0.015 | 0.281 (1.05x) | 0.248 (1.19x) | 0.213 (1.39x) | 0.165 (1.79x) |
| 1000 | 3.343 | 0.067 | 3.044 (1.10x) | 2.308 (1.45x) | 1.618 (2.07x) | 1.018 (3.28x) |
| 3000 | 31.505 | 0.239 | 26.982 (1.17x) | 14.164 (2.22x) | 8.325 (3.78x) | 4.436 (7.10x) |
| 10000 | 370.992 | 0.919 | 183.540 (2.02x) | 74.159 (5.00x) | 38.444 (9.65x) | 19.801 (18.74x) |
| 30000 | 3376.567 | 3.250 | 860.735 (3.92x) | 333.403 (10.13x) | 156.228 (21.61x) | 78.360 (43.09x) |

## clustered scene: force error, mean (worst body)

| N | theta 0.3 | theta 0.5 | theta 0.7 | theta 1 |
| --- | --- | --- | --- | --- |
| 100 | 1.10e-03 (7.56e-02) | 3.69e-03 (7.56e-02) | 9.93e-03 (8.54e-02) | 3.76e-02 (2.82e-01) |
| 300 | 9.16e-04 (1.42e-02) | 6.48e-03 (3.36e-02) | 1.85e-02 (1.69e-01) | 5.44e-02 (3.64e-01) |
| 1000 | 1.13e-03 (1.22e-02) | 7.01e-03 (1.39e-01) | 1.88e-02 (2.52e-01) | 4.98e-02 (6.53e-01) |
| 3000 | 1.39e-03 (1.73e-02) | 7.45e-03 (7.69e-02) | 1.83e-02 (2.53e-01) | 4.94e-02 (3.27e-01) |
| 10000 | 1.58e-03 (2.41e-02) | 8.26e-03 (5.02e-02) | 1.94e-02 (1.06e-01) | 4.95e-02 (3.30e-01) |
| 30000 | 2.19e-03 (2.46e-02) | 9.51e-03 (1.03e-01) | 2.10e-02 (3.80e-01) | 5.13e-02 (1.11e+00) |
