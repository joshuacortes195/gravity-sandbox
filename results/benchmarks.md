# Brute force against Barnes-Hut

Written by `tools/benchmark.cpp`. Same numbers as `benchmarks.json`.

- Machine: Intel Core i5-8259U @ 2.30GHz, 4 physical / 8 logical cores, 8 GB RAM, macOS 15.7.7
- Commit: 8410f78
- Compiler: Apple LLVM 17.0.0 (clang-1700.6.4.2)
- Build flags: Release: -O3 -DNDEBUG
- Softening: 0.01, seed: 1, bodies at rest with equal mass in a ball of radius 1
- Time: one force pass on a single thread, the fastest of at least 3 runs and 0.3 seconds. Tree times include building the tree
- Error: |a_tree - a_brute| / |a_brute| for each body, then the mean and the worst body
- The worst body is measured against its own acceleration. Where the pulls nearly cancel that is small, so the worst reads far higher than the mean

## uniform scene: milliseconds per force pass

| N | brute force | tree build only | theta 0.3 | theta 0.5 | theta 0.7 | theta 1 |
| --- | --- | --- | --- | --- | --- | --- |
| 100 | 0.016 | 0.003 | 0.092 (0.17x) | 0.064 (0.25x) | 0.045 (0.35x) | 0.027 (0.58x) |
| 300 | 0.136 | 0.009 | 0.849 (0.16x) | 0.509 (0.27x) | 0.321 (0.42x) | 0.174 (0.78x) |
| 1000 | 1.499 | 0.051 | 9.117 (0.16x) | 3.704 (0.40x) | 1.856 (0.81x) | 0.891 (1.68x) |
| 3000 | 14.093 | 0.190 | 51.178 (0.28x) | 17.104 (0.82x) | 8.128 (1.73x) | 3.491 (4.04x) |
| 10000 | 164.945 | 0.733 | 257.006 (0.64x) | 76.207 (2.16x) | 36.427 (4.53x) | 14.693 (11.23x) |
| 30000 | 1527.296 | 2.732 | 1096.892 (1.39x) | 311.048 (4.91x) | 131.136 (11.65x) | 56.096 (27.23x) |

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
| 100 | 0.015 | 0.005 | 0.047 (0.32x) | 0.037 (0.40x) | 0.032 (0.47x) | 0.026 (0.59x) |
| 300 | 0.133 | 0.015 | 0.306 (0.43x) | 0.279 (0.48x) | 0.232 (0.57x) | 0.176 (0.76x) |
| 1000 | 1.500 | 0.069 | 3.622 (0.41x) | 2.554 (0.59x) | 1.745 (0.86x) | 1.044 (1.44x) |
| 3000 | 14.008 | 0.239 | 27.957 (0.50x) | 14.737 (0.95x) | 8.528 (1.64x) | 4.600 (3.05x) |
| 10000 | 163.889 | 0.929 | 179.666 (0.91x) | 78.939 (2.08x) | 39.520 (4.15x) | 19.853 (8.26x) |
| 30000 | 1511.687 | 3.422 | 869.352 (1.74x) | 311.490 (4.85x) | 163.634 (9.24x) | 78.704 (19.21x) |

## clustered scene: force error, mean (worst body)

| N | theta 0.3 | theta 0.5 | theta 0.7 | theta 1 |
| --- | --- | --- | --- | --- |
| 100 | 1.10e-03 (7.56e-02) | 3.69e-03 (7.56e-02) | 9.93e-03 (8.54e-02) | 3.76e-02 (2.82e-01) |
| 300 | 9.16e-04 (1.42e-02) | 6.48e-03 (3.36e-02) | 1.85e-02 (1.69e-01) | 5.44e-02 (3.64e-01) |
| 1000 | 1.13e-03 (1.22e-02) | 7.01e-03 (1.39e-01) | 1.88e-02 (2.52e-01) | 4.98e-02 (6.53e-01) |
| 3000 | 1.39e-03 (1.73e-02) | 7.45e-03 (7.69e-02) | 1.83e-02 (2.53e-01) | 4.94e-02 (3.27e-01) |
| 10000 | 1.58e-03 (2.41e-02) | 8.26e-03 (5.02e-02) | 1.94e-02 (1.06e-01) | 4.95e-02 (3.30e-01) |
| 30000 | 2.18e-03 (2.46e-02) | 9.51e-03 (1.03e-01) | 2.10e-02 (3.80e-01) | 5.13e-02 (1.11e+00) |
