# Gravity Sandbox write-up

## What it is

Gravity Sandbox is a gravity N-body simulation that runs in the browser. Thousands of stars, planets and black holes pull on each other, and you can watch two galaxies crash, a black hole tear a star cluster apart, or the three-body figure-eight orbit loop forever. The physics is C++ compiled to WebAssembly, and three.js draws it.

I built it as a portfolio project, and I cared more about the physics being right and every number being measured than about a long feature list. Each phase ended with tests passing, and every number below comes from a file in `results/` that says what machine, settings and commit it was measured on. A couple are readings I took off the page by eye, and the results files say so where they appear.

## What is in it

- A C++ core with brute force gravity, a Barnes-Hut octree for big scenes, and a leapfrog integrator, covered by 44 native tests
- Three WebAssembly builds: plain, SIMD, and SIMD with threads. The page downloads only the one the browser can run
- Five scenes: a mini solar system, the figure-eight orbit, a galaxy collision, a black hole against a star cluster, and a single disk galaxy
- A control panel for pause, speed, tree accuracy and star count, click to drop in a body, and a live energy drift readout
- A black hole that looks like one: a black shadow, a photon ring and gravitational lensing of the stars behind it, drawn with relativity while the gravity itself stays Newtonian

## The decisions, in short

I made these before writing any physics code. The full table with reasons is in [plan.md](plan.md).

- **No collisions.** Bodies never merge. Close passes are handled by softening, so energy stays conserved and the live energy readout is a real correctness check
- **32-bit floats for the bodies, 64-bit for the totals.** three.js can read positions straight out of WebAssembly memory, and SIMD fits 4 floats per instruction. Energy and momentum are summed in 64-bit so rounding does not swamp the readout
- **G = 1 and numbers near 1.** Real units overflow a 32-bit float
- **One fixed timestep for everyone.** Leapfrog only keeps its good long run energy behavior if the timestep never changes
- **Structure of arrays.** One array each for x, y, z, the velocities, the accelerations and the masses. Cache friendly, easy to vectorize, and each array goes to the GPU as its own attribute
- **Out of scope:** collisions, adaptive timesteps, dark matter, relativity, GPU physics, saving scenes, any backend

## The numbers

**The integrator holds its energy.** A two-body orbit run for 1000 orbits, a million steps, stays within about 2 parts in 10,000 of its starting energy. Halving the timestep cuts the energy error by 3.99, which is what a second order method should do. ([energy-drift.md](../results/energy-drift.md))

**Barnes-Hut wins past a few thousand bodies.** One force pass on one thread, theta 0.5, bodies spread evenly through a ball:

| Bodies | Brute force | Barnes-Hut | Speedup | Mean force error |
| --- | --- | --- | --- | --- |
| 1000 | 1.499 ms | 3.704 ms | 0.40x | 0.65% |
| 3000 | 14.093 ms | 17.104 ms | 0.82x | 0.68% |
| 10,000 | 164.945 ms | 76.207 ms | 2.16x | 0.61% |
| 30,000 | 1527.296 ms | 311.048 ms | 4.91x | 0.59% |

([benchmarks.md](../results/benchmarks.md))

**SIMD helps brute force, threads help the tree.** In Chrome 155 on the Mac, SIMD makes brute force 2.44x faster (18.71 to 7.66 ms a step at 2001 bodies) and does nothing for the tree walk, which branches on every cube. 4 threads make the tree 3.26x faster on a 10,000 star disk (24.55 to 7.54 ms a step), and 2.09x to 2.72x on the scenes of 1000 to 2000 bodies. On a 16 thread Windows desktop, 16 threads make the tree 3.00x to 5.08x faster than the plain build and beat 8 and 4 on every scene, so desktops use every core. ([speedups.md](../results/speedups.md))

**Every scene keeps its energy.** Each scene's timestep and softening were picked by running it natively and watching the energy:

| Scene | Bodies | Worst energy drift |
| --- | --- | --- |
| Solar system | 6 | 6.7e-06 over two orbits of the outer planet |
| Figure-eight | 3 | 5.3e-06 over ten periods |
| Galaxy collision | 2002 | 6.7e-04 through the crash |
| Black hole vs cluster | 1001 | 1.4e-03 through the first close pass |
| Disk galaxy | 2001 | 1.5e-05 to t = 10 |

([presets.md](../results/presets.md))

## What went wrong and how I fixed it

**The disk galaxy fell apart into clumps.** My first disk had a tenth of the center's mass, and within one turn it broke into clumps. I thought it was a bug, so I ran it again with brute force instead of the tree and with a timestep four times smaller. Both gave the same clumps, so neither the tree nor the integrator was at fault. It was real physics: a cold, flat disk that pulls on itself is unstable (the Toomre instability). Making the disk 0.005 of the center's mass kept 99.2% of the stars within 10% of their starting distance through the first turn. ([disk-stability.md](../results/disk-stability.md))

**The galaxy collision gained 9% energy at the first pass.** With the same softening the disk uses, 0.01, energy jumped 9% when the galaxies met and climbed to 18%. A more accurate tree did not help and a smaller timestep did, which pointed at the integrator. Stars from one galaxy were getting flung right through the other galaxy's heavy center, where a whole orbit takes only about 3 timesteps, far too few for leapfrog to follow. Raising the softening to 0.05 kept the timestep and brought the drift under 1e-3. ([presets.md](../results/presets.md))

**SIMD did nothing at first.** The first browser timings showed the SIMD build exactly as fast as the plain one, even on brute force, which should be the easy case. The compiler said why: running 4 bodies at once means keeping 4 running totals and adding them at the end, which changes the order of the float adds and so changes the answer slightly. It will not do that unless told it may. A one line `#pragma clang loop vectorize(enable)` on that loop fixed it, and all 44 tests still passed with the reordered sum. ([speedups.md](../results/speedups.md))

**The tree's lead shrank.** That same pragma made native brute force about 2.2x faster too, so my Barnes-Hut benchmark was out of date. Before the fix the tree tied brute force at about 1000 bodies and was 10.69x faster at 30,000. Rerun against the faster brute force, it first wins between 3000 and 10,000 bodies and is 4.91x faster at 30,000. I kept the new numbers, since the old ones were measured against a slower baseline than the one that ships.

**A stall at 8x speed that was most likely the browser.** At 8x the galaxy collision seemed to drop to about 12 fps, with a frame of around a second once a second. Timing each part of the frame showed the page's own work was only about 13 ms a frame and never anywhere near a second. In the window I tested in, frames came about a second apart even when paused, while timers in the same tab ran 100 times a second, because Chrome holds back animation frames in a window that is not in front. That window showed only long gaps, not the bursts of the first reading, so I can't prove it was the same thing. In a window in front the same scene at 8x held about 80 fps by eye. ([browser.md](../results/browser.md))

**A color bug from the graphics pass.** When I added glow, the stars started going through three.js post-processing, which converts colors to the screen's color space at the end. With every effect off the stars skipped that step, so unticking glow made every star darker and more saturated, and phones (which start with glow off) saw different colors from desktops. The star shader now does the conversion itself, so the colors match either way.

## What is not done

- Safari, Firefox and phones are not measured, and I have not checked that Safari draws from views into shared memory
- On the Mac, one reading of the live page did not show threads helping (3.69 ms a step on 4 threads, close to the one thread time), even though the benchmark page does. On the Windows desktop they clearly help the live page, so I have not worked out what was different
- 32-bit positions mean a slow body far from the origin can stop moving, because its step is smaller than the gap between floats there. The scenes stay near the origin to avoid it
