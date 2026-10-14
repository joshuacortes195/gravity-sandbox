# Disk stability

Why the browser's disk galaxy has a disk mass of 0.005. Written by `tools/disk_check.cpp`.

- Machine: Intel Core i5-8259U @ 2.30GHz, 4 physical / 8 logical cores, 8 GB RAM, macOS 15.7.7
- Commit: 8db452b
- Compiler: Apple LLVM 17.0.0, Release: -O3 -DNDEBUG
- Scene: `disk_galaxy` with 2000 stars, center mass 1, radius 1, seed 1, stars between radius 0.1 and 1, no random motion on top of the circles
- "Moved" is the share of stars whose distance from the center changed by more than 10% from where it started
- "Energy drift" is (E - E0) / |E0|. It reads nan for a massless disk, because the total energy there is exactly 0
- One turn of the outer edge takes 2 pi, about t = 6.3. In the browser at 120 steps a second and dt 0.002, t = 3 is about 12.5 seconds

## What breaks the disk

Each row changes one thing from the first row. All run to t = 3.

| Disk mass | Softening | dt | Forces | Moved | Farthest star | Energy drift |
| --- | --- | --- | --- | --- | --- | --- |
| 0.1 | 0.01 | 0.002 | tree, theta 0.7 | 66.2% | 1.39 | 1.96e-04 |
| 0.1 | 0.01 | 0.002 | brute force | 65.5% | 1.42 | -5.17e-07 |
| 0.1 | 0.01 | 0.0005 | tree, theta 0.7 | 66.2% | 1.39 | 1.80e-04 |
| 0.1 | 0.05 | 0.002 | tree, theta 0.7 | 39.1% | 1.14 | 1.90e-04 |
| 0.02 | 0.01 | 0.002 | tree, theta 0.7 | 18.4% | 1.10 | 1.31e-05 |
| 0 | 0.01 | 0.002 | tree, theta 0.7 | 0.0% | 1.00 | nan |

Brute force and a timestep four times smaller give the same result, so the tree and the integrator are not the cause. Only the disk's own mass matters: a cold, flat, self-gravitating disk is unstable and breaks into clumps (the Toomre instability).

## Picking the disk mass

Same settings as the browser (softening 0.01, dt 0.002, tree at theta 0.7), run to t = 6, about one turn of the outer edge.

| Disk mass | Moved | Farthest star | Energy drift |
| --- | --- | --- | --- |
| 0.01 | 9.2% | 1.17 | 2.94e-05 |
| 0.005 | 0.8% | 1.15 | 1.08e-05 |
| 0.001 | 0.0% | 1.01 | 1.73e-07 |

I picked 0.005: the stars still pull on each other, and under 1% of them wander off in the first turn.

## Measure again

```bash
# n, disk mass, softening, dt, theta (below 0 for brute force), steps
./build/disk_check 2000 0.005 0.01 0.002 0.7 3000
```
