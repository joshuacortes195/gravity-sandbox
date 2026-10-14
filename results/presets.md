# Presets

How I picked the timestep and softening for each scene in the browser, and how well each one holds its energy. Written by `tools/preset_check.cpp`, which builds the same scene the page loads and steps it the same way, through the tree.

- Machine: Intel Core i5-8259U @ 2.30GHz, 4 physical / 8 logical cores, 8 GB RAM, macOS 15.7.7
- Commit: 3f0a54c
- Compiler: Apple clang 17.0.0, Release: -O3 -DNDEBUG
- Seed 1 for every random scene, theta 0.7 unless a row says otherwise
- "Energy drift" is (E - E0) / |E0|, with the potential energy summed exactly (brute force) in 64-bit
- ms per step is native, one thread. The browser is slower, see [browser.md](browser.md)

```bash
# name, n, dt, eps, theta, time to run, checkpoints
./build/preset_check collision 1000 0.002 0.05 0.7 15 6
```

## The settings the page uses

| Preset | Bodies | dt | Softening | Ran to t | Worst drift at a checkpoint | ms per step |
| --- | --- | --- | --- | --- | --- | --- |
| Solar system | 6 | 0.01 | 0.001 | 65 (two orbits of the outer planet) | 6.7e-06 | 0.001 |
| Figure-eight | 3 | 0.005 | 0 | 63.26 (ten periods) | -5.3e-06 | 0.000 |
| Galaxy collision, 1000 stars per disk | 2002 | 0.002 | 0.05 | 15 | 6.7e-04 | 3.5 to 8.3 |
| Black hole vs cluster, 1000 stars | 1001 | 0.002 | 0.01 | 10 | -1.4e-03 | 2.6 to 3.0 |
| Disk galaxy, 2000 stars | 2001 | 0.002 | 0.01 | 10 | 1.5e-05 | 2.7 to 2.8 |

Starting energies, to show none of them sits near zero, where a relative drift would blow up for no real reason:

| Preset | Kinetic | Potential | Total |
| --- | --- | --- | --- |
| Solar system | 0.000161866 | -0.000323735 | -0.00016187 |
| Figure-eight | 1.21286 | -2.5 | -1.28714 |
| Galaxy collision | 0.106793 | -0.422147 | -0.315354 |
| Black hole vs cluster (1000) | 0.000904726 | -0.00725022 | -0.0063455 |
| Disk galaxy | 0.00452869 | -0.00907071 | -0.00454202 |

The solar system's total looks tiny, but only because the planets are light. It is exactly half the potential, the way it has to be for circular orbits, so relative to its own size it is far from zero.

## Galaxy collision: why the softening is 0.05

With the softening the single disk uses (0.01), energy jumped by 9% at the first close pass and kept climbing to 18%. 1000 stars per disk unless noted, run to t = 7.5, which is past the closest approach.

| Softening | dt | theta | Drift at t = 5 | Drift at t = 7.5 |
| --- | --- | --- | --- | --- |
| 0.01 | 0.002 | 0.7 (2000 per disk, run to 15) | 9.4e-02 | 1.3e-01 |
| 0.01 | 0.002 | 0.3 | 7.9e-02 | 1.1e-01 |
| 0.01 | 0.0005 | 0.7 | -3.6e-04 | -2.1e-04 |
| 0.05 | 0.002 | 0.7 | 3.2e-04 | 5.6e-04 |
| 0.1 | 0.002 | 0.7 | 1.3e-04 | 3.1e-04 |

A more accurate tree did not help, a smaller timestep did. The cause is stars from one galaxy getting flung right through the other galaxy's heavy center. With softening 0.01 a star that close goes round in about 0.001 time units, which is shorter than the timestep, so leapfrog cannot follow it. A single disk never has that problem because its stars start at radius 0.1. Raising the softening to 0.05 keeps the full speed timestep and brings the drift under 1e-3.

## Black hole vs cluster: the drift comes from the tree

2000 stars, run to t = 3.75, just past the first close pass (at about t = 2.3).

| theta | dt | Drift at t = 2.5 | Drift at t = 3.75 | ms per step |
| --- | --- | --- | --- | --- |
| 0 | 0.002 | 2.3e-05 | -9.6e-07 | 115 to 119 |
| 0.3 | 0.002 | -2.8e-05 | -1.2e-04 | 30 to 40 |
| 0.7 | 0.002 | -4.1e-04 | -9.5e-04 | 6.7 to 7.1 |
| 0.7 | 0.001 | -4.3e-04 | -9.6e-04 | 6.7 to 7.2 |

Halving the timestep changes nothing, and the drift shrinks with theta, so it is the tree's force error and not the integrator. The cluster is tightly packed, so at 2000 stars a step costs 7 ms, more than twice the disk at the same count. That is why the page starts it at 1000 stars.

## Picking the timesteps

- Solar system: the inner planet at radius 0.4 goes round in about 1.6 time units, so dt 0.01 gives it about 160 steps an orbit. At 120 steps a second the outer planet takes about 27 seconds to go round
- Figure-eight: one period is 6.3259, so dt 0.005 is about 1265 steps a loop and one loop takes about 10.5 seconds on screen
- Galaxies and the cluster keep the dt 0.002 from the disk check in [disk-stability.md](disk-stability.md)

## Solar system spacing

My first layout put the Jupiter sized planet at radius 2.2, only 0.7 from the planet at 1.5. Over one orbit of the outer planet that planet's distance from the sun changed by 1.4%. Taking Jupiter's mass away dropped it to 2e-05, and cutting the timestep by 4 changed nothing, so it was Jupiter's pull and not an error. Moving Jupiter out to 3.0 brought the worst planet down to 0.19%, measured by the test in `tests/test_presets.cpp` (brute force, no softening, dt 0.01).
