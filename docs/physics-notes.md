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
| 1000 | 1.499 ms | 3.704 ms | 0.40x | 0.65% |
| 3000 | 14.093 ms | 17.104 ms | 0.82x | 0.68% |
| 10,000 | 164.945 ms | 76.207 ms | 2.16x | 0.61% |
| 30,000 | 1527.296 ms | 311.048 ms | 4.91x | 0.59% |

That is the evenly spread scene. At theta 0.5 the tree first wins somewhere between 3000 and 10,000 bodies, and the clumpy scene is the same (0.95x at 3000, 2.08x at 10,000). Building the tree is a small part of it: 2.7 ms of the 311 ms at 30,000 bodies.

These numbers changed in phase 5. My first run said the two tie at about 1000 bodies and the tree is 10.69x faster at 30,000. Then I found the compiler had never turned the brute force loop into SIMD code (see "SIMD and threads"). Once it did, brute force got about 2.2 times faster natively and the tree barely moved, so the tree's lead shrank. A faster baseline makes the clever method look less impressive, and the honest comparison is against the fastest brute force I can write.

Going from 10,000 to 30,000 bodies makes brute force 9.3 times slower and the tree 4.1 times slower.

Theta trades speed for accuracy. At 30,000 bodies in the even scene, theta 0.3 is 1.39x faster than brute force with 0.13% mean error, 0.5 is 4.91x with 0.59%, 0.7 is 11.65x with 1.3%, and 1.0 is 27.23x with 3.6%.

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

My first browser disk had a tenth of the center's mass and fell apart within one turn: clumps everywhere and stars thrown outward. I checked it was not a bug by changing one thing at a time. Brute force instead of the tree gave the same mess, and so did a timestep four times smaller, while a massless disk stayed perfect and a lighter one broke up more slowly. So it was real physics. A flat disk of stars with no random motion is unstable under its own gravity (the Toomre instability): a spot that is slightly denser than its neighbors pulls them in, gets denser still, and grows into a clump. Real galaxies are kept stable by stars moving randomly on top of their circles and by a dark matter halo. I have neither, so I made the disk light, 0.005 of the center's mass. It still pulls on itself, and it holds its shape for a full turn. That is as far as I checked. A cold disk is unstable at any mass, so a light disk only breaks up more slowly. The numbers are in `results/disk-stability.md`.

Code: `disk_galaxy` in `core/scenes.cpp`, tests in `tests/test_scenes.cpp`, and `tools/disk_check.cpp` for the stability check.

## Running it in the browser

WebAssembly is a compact low level format browsers run at close to native speed. Emscripten compiles my C++ to a `.wasm` file plus a small JavaScript loader. The physics in the browser is the same `core/` code the native tests check. Only `wasm/bindings.cpp` is new, a handful of functions the page can call: load a disk, advance some steps, and tell me where x, y and z sit in memory.

A WASM program gets one big flat block of bytes as its memory, and JavaScript sees it as an `ArrayBuffer`. My position arrays live inside it. `new Float32Array(buffer, pointer, n)` is a view, a window onto those bytes, so three.js reads the positions where they already are. Nothing is copied on the JavaScript side. The GPU upload still happens every frame.

Two things can kill a view. If the WASM memory grows, the old buffer is thrown away and every view on it goes dead. If a `std::vector` reallocates, its data moves to a new address. So the memory size is fixed at 64 MB and never grows, and the page grabs fresh views after anything that changes the body count.

The physics runs on a fixed timestep. The browser draws a frame roughly every 16 ms, but never exactly, and leapfrog only keeps energy bounded when dt never changes. So each frame I add the real time that passed to a backlog, and run one physics step for every 1/120 of a second in it. Drawing just shows whatever the state is. There is a cap on how much physics one frame can do. Without it, one slow frame would need extra steps, which makes the next frame slower, which needs even more steps. In phase 3 the cap was 8 steps a frame. Once the speed slider came in (phase 4), a fixed count stopped making sense, so now each frame steps until it is done or has spent 12 ms on physics, whichever comes first. Past that the leftover time is dropped and the sim slows down instead of the frame rate falling apart.

Every body is drawn as one GPU point. The shader gets x, y and z as three separate inputs, one per array, and puts them together into a position. three.js normally works out a bounding sphere from a single position array to decide if something is on screen. It cannot do that here, so that check is off and I tell it how many points to draw.

Code: `wasm/bindings.cpp`, `web/src/main.ts`.

## The figure-eight orbit

Three equal masses chase each other around one figure-eight shaped curve, each a third of a period behind the next. An orbit where every body follows the same path is called a choreography. Chenciner and Montgomery proved in 2000 that this one exists, and Carles Simó worked out the starting numbers:

    x1 = -x2 = (0.97000436, -0.24308753),  x3 = (0, 0)
    v3 = (-0.93240737, -0.86473146),  v1 = v2 = -v3 / 2
    period T = 6.32591398,  all masses 1,  G = 1

The middle body moves twice as fast as the outer two and the other way, so the total momentum is zero and the curve stays put. The total energy is -1.287 and the angular momentum is exactly zero.

The three-body problem is chaotic in general, so why does this one survive? It is linearly stable: a small nudge stays small instead of growing. That is why it keeps its shape in my sim with 32-bit floats. My tests run it with no softening and brute force. After one period every body is back within 7.5e-05 of where it started, and after a third of a period each body sits where the next one started (within 4.6e-04, most of which is the step count not landing exactly on T / 3).

Code: `figure_eight` in `core/scenes.cpp`, tests in `tests/test_presets.cpp`.

## The mini solar system

A sun of mass 1 and five planets on circular orbits at radius 0.4, 0.7, 1.0, 1.5 and 3.0. The planet masses are roughly the real ratios, with a Jupiter of 9.5e-4 on the outside. Each planet gets the circular speed sqrt(G * (M + m) / r), then everything gets one shared nudge so the total momentum is zero. That makes the sun wobble a little against Jupiter, like the real one.

My first layout had Jupiter at 2.2, and the planet at 1.5 drifted 1.4% off its circle in one Jupiter orbit. That was not a bug. Taking Jupiter's mass away fixed it, a smaller timestep did not. Planets that close together really do tug each other's orbits out of shape. Moving Jupiter to 3.0 brought it down to 0.19%. The real Jupiter is about 3.4 times farther out than Mars for a reason.

## Galaxy collision and plunging orbits

Two of my disk galaxies start some distance apart on a stretched orbit around each other. I want to choose how close they get, so the setup works backwards from that. At the far point of an orbit (r_a) a body moves straight sideways, and two things are conserved on the way to the near point (r_p): energy and angular momentum. Solving those two together gives the sideways speed at the far point as a fraction of the circular speed:

    v = sqrt(2 * r_p / (r_a + r_p)) * sqrt(G * M / r_a)

My `two_body_orbit` already takes that fraction, so I build two points on that orbit and drop each whole galaxy onto one of them, recentered so its center of mass lands exactly on the point. The second galaxy is tilted by one radian so the crash is not flat. The first disk spins the same way as the orbit, which is the setup that pulls out the longest tidal tails.

The page uses r_a = 2.5 and r_p = 0.8. My test swaps each galaxy for a single point with its mass and motion, runs them, and the closest approach comes out 0.800.

The collision broke something the single disk never did: energy jumped by 9% at the close pass. A more accurate tree did not help, a smaller timestep did. Stars from one galaxy get flung right through the other galaxy's heavy center, and with softening 0.01 a star that close goes round in about 0.0063 time units, only about 3 timesteps. Leapfrog needs far more steps than that per orbit. Raising the softening to 0.05 fixed it at full speed (drift 6.7e-04 over the whole run).

## Black hole against a star cluster

A black hole of mass 1 and a ball of stars with total mass 0.01 and radius 0.1, on the same kind of plunging orbit, from 1.5 out to 0.15.

A ball of stars at rest collapses, and one with too much speed flies apart. It holds its size when the motion balances the pull. That balance is the virial theorem: 2K = -W, kinetic energy against potential energy. For an even ball of mass M and radius R the potential energy is -3/5 * G * M^2 / R. If I pick each star's velocity evenly inside a ball of speeds with radius sqrt(G * M / R), the average speed squared is 3/5 of that radius squared, and the balance comes out exactly. My test measures 2K / |W| = 0.99 for 2000 stars. This is not a true steady state, the ball will slosh around a little, but it does not collapse or explode.

What tears it apart is the tide. Near the black hole, the side of the cluster facing it gets pulled harder than the far side. When that difference beats the cluster's own pull, the cluster stretches into a stream. A rough rule is the tidal radius, r_p * (m / 3M)^(1/3), which is 0.15 * (0.01 / 3)^(1/3) = 0.022 here. The cluster's radius is 0.1, so it gets shredded.

The energy drift here, about 1e-3, comes from the tree. Halving the timestep does nothing, and with 2000 stars at t = 3.75 the drift drops from -9.5e-04 at theta 0.7 to -9.6e-07 at theta 0. A tightly packed cluster is the tree's worst case: every star is close to many others, so little can be lumped together. That also makes it slower per body than the disk.

## The live energy readout

Drift is (E - E0) / |E0|. Dividing by E0 breaks when E0 is near zero, because then a tiny change looks huge. I checked every preset's starting energy and none is near zero relative to its own kinetic and potential energy (`results/presets.md`).

The potential energy is a sum over every pair, O(n^2): 50 million pairs at 10,000 bodies, far too slow for one frame. So the page freezes a copy of the bodies and adds up a few rows of the sum per frame, about 300,000 pairs worth, while the real sim keeps moving. When the last row is in, the readout updates. A new sum starts once a second has passed since the last one started. The copy matters: adding rows from bodies that keep moving would mix different moments and the total would mean nothing.

Spawning a body adds energy that was never in the system, so the drift would jump for no fault of the physics. A spawn resets the baseline: the next finished sum becomes the new zero. A spawned body starts on a circular orbit around everything, at sqrt(G * M_total / r), so it joins the scene instead of falling straight in. It also brings momentum the scene did not have, so after a click the whole scene slowly drifts. That shows most in the figure-eight, where a spawned body weighs 0.3 against a total of 3.

Code: `potential_rows` in `core/diagnostics.cpp`, `energy_begin` and `energy_work` in `wasm/bindings.cpp`, the readout in `web/src/main.ts`.

## SIMD and threads

These two speedups have nothing to do with physics. They make the same math run faster, and the answer has to stay the same.

SIMD means single instruction, multiple data. One CPU instruction works on 4 floats side by side instead of one. I do not write SIMD code by hand. Building with `-msimd128` lets the compiler rewrite plain loops so each pass handles 4 bodies. It can only do that when the same math runs on neighboring entries of an array with no branches in the way. Brute force is exactly that: one flat loop over every other body j, reading x[j], y[j], z[j] and m[j] straight down the arrays. This is where storing bodies as structure of arrays pays off. The tree walk is different. It jumps around the tree, and every cube is a yes or no on the opening rule, so there is far less for the compiler to line up.

Threads are separate workers running at the same time on different cores. Each step has two parts. The tree build stays on one thread. The walk is where the time goes, and it splits cleanly: once the tree is built, every body's walk only reads the tree and only writes its own ax[i], ay[i], az[i]. No two threads write the same thing, so there are no locks. Threads grab blocks of 64 bodies from a shared counter until none are left. A thread that drew cheap bodies far from everything just takes more blocks. Each body still adds up its pulls in the same order as on one thread, so the answer matches one thread bit for bit, and there is a test for that.

The browser makes threads awkward in two ways. First, threads in WASM share one memory, a `SharedArrayBuffer`, and browsers only allow that on a page that is cross-origin isolated. That needs two headers on every response, COOP and COEP, which `vite.config.ts` sends locally and `web/vercel.json` sends on the live site. If a page is not isolated, it gets the single thread SIMD build instead. Second, a WASM thread is a web worker, and creating one needs the page's main thread to come back to the browser first. My physics runs on the main thread inside a frame, so it cannot wait for a worker to start. The fix is to start every worker once while the page loads, and keep the helper threads asleep between steps. While the helpers work, the main thread does its share and then spins until they finish, because the main thread is not allowed to sleep.

My first measurement showed SIMD doing nothing, even for brute force (18.61 ms against 18.52 ms per step). The compiler can explain itself: `-Rpass-missed=loop-vectorize` said "cannot prove it is safe to reorder floating-point operations". The loop adds every pull into one running total. Doing 4 bodies at once means 4 running totals added together at the end, which is a different order of adds. Float addition is not exactly associative, (a + b) + c can differ from a + (b + c), usually in the last few bits and by more when big numbers nearly cancel, so the compiler will not reorder on its own. `#pragma clang loop vectorize(enable)` on that one loop gives it permission there and nowhere else. `-ffast-math` would also work, but it loosens the float rules for the whole program, including my energy sums. After the pragma, brute force ran 2.44x faster in the browser and about 2.2x faster natively.

What I measured in Chrome on my laptop (full table in `results/speedups.md`):

- SIMD: brute force at 2001 bodies went from 18.71 to 7.66 ms per step (2.44x). The tree scenes did not change (0.98x to 1.03x)
- Threads: the 10,000 star disk went from 24.55 ms on one thread to 12.57 ms on 2 (1.95x) and 7.54 ms on 4 (3.26x). The scenes of 1000 to 2000 bodies gain less, 2.09x to 2.72x on 4 threads. That is Amdahl's law: the part you did not split sets the limit. Here the unsplit part is the tree build, the leapfrog update and the fixed cost of waking the helpers and waiting for them every step. The build alone is only about 3% of a pass, so it is not the main cause. I did not measure the split between the other two

Questions I should be able to answer:

- Why does the threaded answer match one thread exactly? Each body's pulls are still added in the same order by one thread. Only which thread handles which body changes
- Why not 8x on 8 logical cores? There are 4 physical cores, the other 4 are hyperthreads sharing them, Chrome only reported 4, and the tree build, the leapfrog update and the hand off to the helpers are not split
- Why did SIMD help brute force and not the tree? Brute force is a straight loop over arrays. The tree walk is a chain of branches and jumps
- Why does a faster brute force change the Barnes-Hut result? The tree's speedup is measured against brute force. A better baseline means a smaller, more honest speedup

There is one build per flag set: plain (the baseline I measure against), SIMD, and SIMD plus threads. Each one needs its own copy of the core, because with threads every file has to be built with `-pthread`.

Code: `core/threads.cpp`, the walk in `compute_accel_tree` in `core/octree.cpp`, the three builds in `CMakeLists.txt`, the build picked in `web/src/main.ts`, the benchmark page in `web/src/bench.ts`.
