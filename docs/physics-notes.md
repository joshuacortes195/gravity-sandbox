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

## Barnes-Hut and theta

Brute force compares every body with every other body, so doubling N makes it four times slower. Barnes-Hut gets around that with one observation: a clump of stars that is far away pulls on me almost exactly like a single body with the clump's total mass, sitting at its center of mass. I do not need each star in it.

To find clumps at every scale I build an octree. I put all the bodies in one cube. Any cube with more than one body in it gets cut into 8 smaller cubes, halved along x, y and z, and its bodies are sorted into them. This repeats until each cube holds one body or none. Every cube remembers the total mass inside it and the center of mass, which is the average position weighted by mass.

To get the force on one body I start at the root cube and ask one question: is this cube small and far away? The test is

    width of the cube / distance to its center of mass < theta

If yes, the whole cube counts as one body. If no, I open it and ask the same thing of its 8 children. A leaf cube is just a body, so its pull is added directly with the same softened formula brute force uses.

Theta is the accuracy dial. At theta = 0 no cube ever passes, every cube is opened down to single bodies, and the answer is brute force. A bigger theta accepts bigger and closer cubes, which is faster and less accurate. Around 0.5 is the usual choice.

Why it is O(n log n): the cubes a body has to open are the ones near it. Far away, one cube stands in for thousands of bodies. The tree is about log n levels deep and a body does a roughly fixed amount of work per level, so n bodies cost about n log n.

Details in my version:

- The tree is rebuilt from scratch every step. Bodies move, and building is cheap next to the force pass.
- A cut is stopped after 24 levels. Two bodies on the exact same point can never be separated by cutting, so without a limit the tree would split forever. Bodies that reach the limit share a leaf as a short chain.
- A body never uses a cube it is sitting inside as one far away body. The theta test measures distance to the center of mass, and with a big theta that point can be far enough away to pass even though the cube contains the body itself. Then the body's own mass would pull on it. I found this with two bodies of mass 1 and 3 at opposite corners of a cube: at theta = 1 the light one got a pull of 1.37 per axis where the right answer is 0.577. My first fix compared the body's position with the cube's edges. That still failed for bodies packed close together far from the origin, because the edges of very small cubes round off in 32-bit and a body can land a hair outside the cube it was sorted into. The walk now follows the body's own path down the tree, using the same sorting rule the build used, and always opens the cubes on that path.
- Each cube's mass is added up from its 8 children, not body by body. Adding 30,000 small masses one after another in 32-bit gave a root mass of 0.99968 where the true total is 1. Summing cube by cube gives 1.
- Tree forces break Newton's third law slightly. Body A might see B's cube as one lump while B sees A exactly, so the two pulls are not equal and opposite. Momentum and energy are no longer conserved to rounding the way they are with brute force.

Code: `core/octree.h` and `core/octree.cpp`. `Octree::build` makes the tree and `compute_accel_tree` walks it. The tests in `tests/test_octree.cpp` compare it against brute force.

What I measured, on one thread with softening 0.01 (full tables in `results/benchmarks.md`):

| N | brute force | tree at theta 0.5 | speedup | mean force error |
| --- | --- | --- | --- | --- |
| 1000 | 3.367 ms | 3.396 ms | 0.99x | 0.65% |
| 3000 | 31.461 ms | 16.376 ms | 1.92x | 0.68% |
| 10,000 | 369.842 ms | 78.959 ms | 4.68x | 0.61% |
| 30,000 | 3351.005 ms | 313.357 ms | 10.69x | 0.59% |

That is the evenly spread scene. The tree and brute force tie at about 1000 bodies there. On the clumpy scene the tree is already ahead at 300 bodies (1.19x). Building the tree is a small part of it: 2.6 ms of the 313 ms at 30,000 bodies.

Going from 10,000 to 30,000 bodies makes brute force 9.1 times slower and the tree 4.0 times slower.

Theta trades speed for accuracy. At 30,000 bodies in the even scene, theta 0.3 is 3.0x faster than brute force with 0.13% mean error, 0.5 is 10.7x with 0.59%, 0.7 is 24.8x with 1.3%, and 1.0 is 62.5x with 3.6%.

The worst single body is much further off than the mean, up to 111% at theta 1 and 19% at theta 0.5 in these runs.

Questions I should be able to answer:

- Why is the error small? Treating a clump as a point ignores its shape. That mistake shrinks like (size / distance)^2, and theta caps size / distance.
- How do I know the tree is right? At theta = 0 it matches brute force to rounding, and the error against brute force shrinks every time theta shrinks. Both are tests.
- Why rebuild every step instead of updating the tree? Simpler, and the build is a small part of the time.
- When does brute force win? At small N, where the tree's overhead costs more than it saves.
- Why does the worst body have a much bigger error than the average? The error is measured against each body's own acceleration. A body near the middle of a ball is pulled almost equally from every side, so its net acceleration is small and a normal sized mistake looks large next to it.
- Why an array of nodes instead of one array per field like the bodies? The walk reads all of a node's fields together, so keeping them side by side is the cache friendly layout here.

## A disk of stars on circular orbits

A ball of bodies at rest just falls in on itself, so for the browser I needed a scene where things go around. A star moves in a circle when gravity's pull is exactly what it takes to bend its path into that circle:

    v^2 / r = G * M / r^2,  so  v = sqrt(G * M / r)

M is the mass pulling it inward. Every star gets that speed, pointing sideways to the line from the center, so it keeps falling around the center instead of into it.

Two details made it work in my sim:

- Softening weakens the pull close in, so the speed has to match the softened force, `v^2 = G * M * r^2 / (r^2 + eps^2)^1.5`. With the plain formula the inner stars start too fast. In my test with softening 0.05, stars on the plain speed swung in and out by up to 50% of their radius in one inner orbit. With the softened speed the worst change was 0.00056%.
- The disk has mass of its own. I count the center plus the share of the disk inside the star's radius. Stars are spread evenly over the ring's area, so that share is the same random number that picked the radius. I treat that mass as if it sat at the center, which is only exact for a round ball, not a flat disk. For a disk that is light next to its center it is close.

Random star positions leave a little leftover momentum, which would make the whole galaxy drift across the screen. The center gets a small velocity the other way to cancel it.

My first browser disk had a tenth of the center's mass and fell apart within one turn: clumps everywhere and stars thrown outward. I checked it was not a bug by changing one thing at a time. Brute force instead of the tree gave the same mess, and so did a timestep four times smaller, while a massless disk stayed perfect and a lighter one broke up more slowly. So it was real physics. A flat disk of stars with no random motion is unstable under its own gravity (the Toomre instability): a spot that is slightly denser than its neighbors pulls them in, gets denser still, and grows into a clump. Real galaxies are kept stable by stars moving randomly on top of their circles and by a dark matter halo. I have neither, so I made the disk light, 0.005 of the center's mass. It still pulls on itself, and it holds its shape for a full turn. The numbers are in `results/disk-stability.md`.

Code: `disk_galaxy` in `core/scenes.cpp`, tests in `tests/test_scenes.cpp`, and `tools/disk_check.cpp` for the stability check.

## Running it in the browser

WebAssembly is a compact low level format browsers run at close to native speed. Emscripten compiles my C++ to a `.wasm` file plus a small JavaScript loader. The physics in the browser is the same `core/` code the native tests check. Only `wasm/bindings.cpp` is new, a handful of functions the page can call: load a disk, advance some steps, and tell me where x, y and z sit in memory.

A WASM program gets one big flat block of bytes as its memory, and JavaScript sees it as an `ArrayBuffer`. My position arrays live inside it. `new Float32Array(buffer, pointer, n)` is a view, a window onto those bytes, so three.js reads the positions where they already are. Nothing is copied on the JavaScript side. The GPU upload still happens every frame.

Two things can kill a view. If the WASM memory grows, the old buffer is thrown away and every view on it goes dead. If a `std::vector` reallocates, its data moves to a new address. So the memory size is fixed at 64 MB and never grows, and the page grabs fresh views after anything that changes the body count.

The physics runs on a fixed timestep. The browser draws a frame roughly every 16 ms, but never exactly, and leapfrog only keeps energy bounded when dt never changes. So each frame I add the real time that passed to a backlog, and run one physics step for every 1/120 of a second in it. Drawing just shows whatever the state is. There is a cap of 8 steps per frame. Without it, one slow frame would need extra steps, which makes the next frame slower, which needs even more steps. With the cap the extra time is dropped and the sim slows down for a moment instead of locking up.

Every body is drawn as one GPU point. The shader gets x, y and z as three separate inputs, one per array, and puts them together into a position. three.js normally works out a bounding sphere from a single position array to decide if something is on screen. It cannot do that here, so that check is off and I tell it how many points to draw.

Code: `wasm/bindings.cpp`, `web/src/main.ts`.

## Still to come

I will add a section for each of these as I build it:

- The figure-eight orbit (phase 4)
- SIMD and threads (phase 5)
