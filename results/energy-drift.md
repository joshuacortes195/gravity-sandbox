# Two-body energy drift

Measured on 2026-10-13 with `build/run_scene` at commit `a5e5d10`. I ran all five again after the follow-up fixes to the force guard and the CLI arguments, and every number came out the same.

## Setup

| | |
| --- | --- |
| Scene | Two bodies, masses 0.75 and 0.25, starting 1 unit apart, orbiting in the xy plane |
| N | 2 |
| Force | Brute force, softening 0 |
| Integrator | Leapfrog kick-drift-kick, fixed timestep |
| Precision | 32-bit state, energy and momentum summed in 64-bit |
| Build | Release, `-O3 -DNDEBUG`, Apple clang 17.0.0, C++17 |
| Machine | Intel Core i5-8259U @ 2.30GHz, macOS 15.7.7 |

Speed scale 1 is a circular orbit with period 2 * pi. Speed scale 0.7 starts the bodies at the far point of a stretched orbit (eccentricity 0.51) with period 3.386214. The timestep is the period divided by the steps per orbit.

Energy drift is (E - E0) / E0. "Worst" is the largest size it reached at any step of the run. "Miss" is how far the light body ended from where it started, after a whole number of orbits.

## Results

| Speed scale | Steps per orbit | Timestep | Orbits | Final energy drift | Worst energy drift | Momentum change | Miss |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | 1000 | 0.00628319 | 100 | -3.951e-05 | 4.773e-05 | 1.971e-06 | 2.814e-02 |
| 1 | 1000 | 0.00628319 | 1000 | -8.505e-05 | 1.029e-04 | 2.185e-05 | 4.908e-01 |
| 0.7 | 1000 | 0.00338621 | 100 | -1.325e-05 | 1.228e-04 | 7.707e-06 | 1.565e-02 |
| 0.7 | 1000 | 0.00338621 | 1000 | -1.636e-04 | 2.064e-04 | 4.993e-06 | 4.309e-01 |
| 0.7 | 4000 | 0.000846554 | 100 | 5.741e-07 | 5.030e-05 | 5.876e-06 | 6.813e-03 |

Commands, in row order:

```bash
./build/run_scene 1 1000 100
./build/run_scene 1 1000 1000
./build/run_scene 0.7 1000 100
./build/run_scene 0.7 1000 1000
./build/run_scene 0.7 4000 100
```

## What the numbers say

Energy holds to about 2 parts in 10,000 over 1000 orbits and a million steps.

The drift is not perfectly flat. Going from 100 to 1000 orbits roughly doubles the worst drift on both orbits. Leapfrog's own error does not grow like that, so I put this down to 32-bit rounding: every step rounds the positions and velocities, and those small errors add up slowly. I have not confirmed it by running the same scene in 64-bit.

The miss grows much faster than the energy error, from about 0.03 after 100 orbits to about 0.5 after 1000. The orbit keeps its shape and energy but the bodies slip along it. So after a long run I can trust the shape of an orbit more than where a body is on it.

## Convergence check from the tests

`tests/test_integrator.cpp` prints these when run with `./build/tests -s`. Same scene with speed scale 0.7.

| Steps per orbit | Orbits | Worst energy drift |
| --- | --- | --- |
| 200 | 1 | 2.868e-03 |
| 400 | 1 | 7.182e-04 |
| 500 | 10 | 4.603e-04 |
| 500 | 100 | 4.603e-04 |

Halving the timestep cut the error by 3.99, which is what a second order method should do. At 500 steps per orbit the worst drift was the same over 10 and 100 orbits, because at that coarser timestep leapfrog's bounded error is bigger than the rounding.
