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

## How the bodies are stored

Each body has a position, a velocity, an acceleration and a mass. I keep one array of every x, one array of every y, and so on, ten arrays in all. This is called structure of arrays. The other way round, one struct per body, is array of structs.

The force loop reads x, y, z and mass for thousands of bodies in a row. With one array per field those values sit next to each other in memory, so the CPU cache is full of what the loop needs and nothing else. Later it also lets one SIMD instruction work on 4 bodies at once, and lets three.js read the x array straight out of WASM memory.

Code: `core/bodies.h`.

Questions I should be able to answer:

- Why structure of arrays? Cache use, SIMD, and handing whole arrays to the renderer without copying.
- What is the downside? Adding or removing one body touches ten arrays, and the code for one body is spread out.

## The gravity force

Every body pulls every other body toward it. The acceleration of body i caused by body j is

    a = G * m_j / r^2, pointing from i toward j

where r is the distance between them. Twice the mass gives twice the pull. Twice the distance gives a quarter of the pull.

The mass of body i itself is missing from that formula on purpose. The force on i is G * m_i * m_j / r^2, and acceleration is force divided by m_i, so it cancels. That is why a feather and a hammer fall at the same rate.

To get the direction I take the arrow from i to j, (dx, dy, dz), and divide by its length r. Put together that is m_j * (dx, dy, dz) / r^3, which is why the code divides by `r2 * sqrt(r2)`.

The total acceleration of a body is the sum of the pulls from all the others. With N bodies that is about N * N pairs every step. This is the brute force or O(n^2) method. It is exact, and it is too slow for big N: 10 times the bodies costs 100 times the work. Barnes-Hut in phase 2 is the fix.

Code: `compute_accel` in `core/gravity.cpp`.

Questions I should be able to answer:

- Why is it O(n^2)? Every body needs the pull from every other body.
- Why loop over every i and j and not each pair once? Each pair once would halve the work, but then two bodies write into each other's totals. The full loop only writes to body i, which is simpler to vectorize and to split across threads later.
- How do I know the force is right? Tests check it against pairs worked by hand, check that all the forces in a system cancel out, and check that it matches the slope of the potential energy.

## Softening

When two bodies get very close, r^2 gets tiny and the force heads for infinity. With a fixed timestep, one step with a huge force throws both bodies away at a speed they never should have had, and the energy of the whole sim is wrong from then on.

Softening replaces r^2 with r^2 + eps^2, where eps is a small length I choose. This form is called Plummer softening. Far apart it changes almost nothing. Close in, the force stops growing, and it falls to zero when the bodies sit on top of each other. It is like treating each body as a small fuzzy ball and not a point.

With softening off, two bodies at exactly the same point would give 0 / 0. The code treats a pair at zero distance as no pull at all. The same check covers a gap so small that a 32-bit float rounds its cube to zero (around 1e-15), which would otherwise give an infinite force.

Code: the `r2` and `pull` lines in `core/gravity.cpp`.

Questions I should be able to answer:

- What does softening cost? Anything that happens at distances below eps is wrong. Tight pairs and close flybys are smoothed out.
- How do I choose eps? Small compared to the usual gap between bodies, and large enough that the timestep can follow the closest pass.
- Why not merge bodies that collide? I chose softening only, so energy stays conserved and the energy readout is a real check.

## Integrators and leapfrog

The force gives the acceleration right now. An integrator is the rule for stepping forward a small time dt.

The simplest rule is Euler: move each body by v * dt, then change v by a * dt. It uses only the values from the start of the step, and on an orbit it gains energy every step, so orbits spiral outward.

I use leapfrog in its kick-drift-kick form:

1. kick: change the velocity by half a step of acceleration
2. drift: move the position a full step using that new velocity
3. work out the forces at the new positions
4. kick: change the velocity by the other half step

It costs one force pass per step, the same as Euler. It has two properties Euler does not:

- It is second order. Halving dt cuts the error by 4. I measured 3.99.
- It is time reversible. Flip every velocity and it walks back along its own path. Because of that, its energy error wobbles up and down but does not pile up. This only holds while dt stays fixed.

`step` needs the accelerations to be current when it is called, so I call `compute_accel` once after setting up a scene. After that, each step leaves fresh accelerations behind for the next one.

When I swapped leapfrog for a plain full kick then drift as an experiment, the halving ratio fell to 2.03 and the worst energy drift went from 4.6e-4 to 1.9e-2.

Code: `step` in `core/integrator.cpp`.

Questions I should be able to answer:

- Why not Runge-Kutta 4? It is more accurate per step, but its energy error grows steadily over a long run and it needs 4 force passes per step. For orbits that have to last thousands of periods, an error that stays bounded is worth more than one that starts small.
- What does symplectic mean? It is the name for integrators like leapfrog that keep the structure of the physics, which is what stops the energy error from piling up.
- Why a fixed timestep? Changing dt from step to step breaks the reversibility that keeps the energy error bounded.

## Energy and momentum

Nothing outside the sim pushes on it, so three totals should never change.

- Energy. Kinetic energy is 0.5 * m * v^2 for each body. Potential energy is -G * m_i * m_j / r for each pair. It is negative because zero is defined as "infinitely far apart", and I would have to add energy to pull two bodies apart. As bodies fall toward each other the potential drops, the kinetic rises, and the total stays the same.
- Momentum. m * v added over every body. Every pull has an equal pull back, so the total cannot change.
- Angular momentum. m * (r cross v) added over every body, which measures how much the system spins about the origin. Gravity pulls along the line between two bodies, so it cannot add spin.

These are my correctness checks. I do not need to know the right answer for a scene. I only need the totals to stay flat. Energy drift is (E - E0) / E0.

The potential energy has to use the same softened distance as the force, -m_i * m_j / sqrt(r^2 + eps^2). Force is the downhill slope of potential energy, and if the two do not match, energy is not conserved even with a perfect integrator. One of my tests nudges a body a tiny distance, measures how the potential changes, and checks that it gives the force back.

Positions and velocities are 32-bit, but these sums are done in 64-bit so rounding in the sum itself does not swamp the readout.

What I measured on a two-body orbit: energy stays within about 2 parts in 10,000 over 1000 orbits. It is not perfectly flat. The worst drift roughly doubles between 100 and 1000 orbits, which I put down to 32-bit rounding in the positions and velocities. The position along the orbit slips much more than the energy does: the light body is about 0.5 away from where it should be after 1000 orbits. The table is in `results/energy-drift.md`.

Code: `core/diagnostics.cpp`.

Questions I should be able to answer:

- How do I know the simulation is correct? Conserved totals stay flat, known orbits come back to their start after one period, and the error shrinks by 4 when I halve the timestep.
- Why is potential energy negative? Zero is set at infinite separation, and bound bodies have less energy than that.
- Why is the energy check O(n^2)? Potential energy is a sum over every pair.
- Energy looks fine, so is the sim exact? No. Energy can hold while a body's place along its orbit drifts.

## Still to come

I will add a section for each of these as I build it:

- Barnes-Hut and theta (phase 2)
- Fixed timestep in the browser (phase 3)
- The figure-eight orbit (phase 4)
- SIMD and threads (phase 5)
