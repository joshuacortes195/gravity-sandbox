# Physics notes

My notes on the physics in this project, written as I learn it. Each section says what the idea is, where the code for it lives, and what I would expect to be asked about it.

## The N-body problem

N bodies each pull on every other body with gravity. For two bodies there is an exact formula for the orbits. For three or more there is no general formula, so the only way to find out what happens is to simulate it: work out the forces, move everything forward by a small step of time, and repeat.

That loop is the whole project. The rest is about making each step accurate (the integrator), fast (Barnes-Hut, SIMD, threads) and checkable (energy and momentum).

## Units: G = 1

Newton's law has a constant in it, G, that converts mass and distance into force. In metric units it is 6.674e-11, which is tiny, while the mass of the Sun is about 2e30 kg. A 32-bit float cannot hold anything bigger than about 3.4e38, and the force formula multiplies two masses together. Two Suns give 4e60, which overflows to infinity.

N-body codes avoid this by picking their own units so that G is exactly 1. I am not changing the physics, only the rulers. If I measure mass in "sim masses" and distance in "sim lengths", time comes out in "sim times", and all three stay close to 1.

One result I will use in the tests: with G = 1, a very light body circling a mass M at distance r moves at speed sqrt(M / r) and takes 2 * pi * sqrt(r^3 / M) to go around once. With M = 1 and r = 1 the speed is 1 and the period is 2 * pi. When both bodies have real mass they circle their shared center, and the same period formula works with M as the total mass and r as the distance between them. Both formulas assume no softening.

Code: `core/units.h` defines `G`.

Questions I should be able to answer:

- Why not use real units? Because the numbers overflow a 32-bit float. Scaled units keep everything near 1.
- Does setting G = 1 change the result? No. It is a change of units. I can convert back by choosing what one sim mass and one sim length stand for.

## Still to come

I will add a section for each of these as I build it:

- The gravity force (phase 1)
- Softening (phase 1)
- Integrators and leapfrog (phase 1)
- Energy and momentum conservation (phase 1)
- Barnes-Hut and theta (phase 2)
- Fixed timestep in the browser (phase 3)
- The figure-eight orbit (phase 4)
- SIMD and threads (phase 5)
