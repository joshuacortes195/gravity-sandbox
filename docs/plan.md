# Plan

Gravity Sandbox is a gravity N-body simulation with a C++ core compiled to WebAssembly. Anyone can open the live demo and watch thousands of bodies pull on each other.

It is a portfolio project. I care more about correct physics, measured numbers and a clean demo than about a long feature list.

## Decisions

I made these before writing any physics code.

| Decision | Choice | Why |
| --- | --- | --- |
| Close passes | Softening only. Bodies never merge or collide | Energy stays conserved, so the live energy drift readout is a real correctness check. This only holds if potential energy uses the same softened distance as the force. A black hole can still pull a cluster apart with tides alone |
| Number format | Positions, velocities and masses are 32-bit floats. Energy and momentum are summed in 64-bit | three.js can read 32-bit positions straight out of WASM memory with no copy on the JavaScript side (the upload to the GPU still happens every frame), and SIMD fits 4 floats in one instruction. Summing diagnostics in 64-bit keeps the readout from being dominated by rounding |
| Units | G = 1, with masses, distances and times near 1 | Standard for N-body codes. In metric units the numbers overflow a 32-bit float, which tops out near 3.4e38. Values near 1 avoid that, and the published figure-eight starting conditions already use these units |
| Timestep | One fixed timestep for every body | Leapfrog only keeps its good long-run energy behavior when the timestep does not change |
| Galaxies | A heavy central body with a disk of light stars on circular orbits. Every star still pulls on every other | Gives tidal tails and bridges, and I can explain every line of the setup. No dark matter halo |
| Devices | Desktop first. Phones get the plain single-thread build, a lower default N and touch camera controls | Threads need shared memory, which browsers only allow on pages served with the cross-origin isolation headers. A fallback build covers any case where that is missing |
| Tests | doctest, vendored as one header in `third_party/` | No package manager and no network needed to build |
| Body storage | Structure of arrays: one array each for x, y, z, vx, vy, vz, ax, ay, az and mass | Cache friendly, easy to vectorize, and each array can be handed to the GPU as its own attribute |

## Out of scope

- Collisions, merging and black holes swallowing bodies
- Adaptive or per-body timesteps
- Dark matter halos, gas, relativity
- Running the physics on the GPU
- Saving, loading or sharing scenes
- Accounts or any backend

## Layout

```
core/         C++ physics core
tests/        native tests (doctest)
third_party/  doctest header
tools/        native CLI and benchmark harness (phases 1 and 2)
wasm/         the functions the page calls into the core (phase 3)
web/          Vite + TypeScript app (phase 3)
docs/         this plan and my physics notes
results/      measured numbers and how each was measured
```

## Milestones

Each phase ends with tests passing, a commit and a push.

### Phase 0: plan and setup (done)

- Scope decisions above
- Repo, CMake, doctest runner, folder layout
- Done when: `ctest` runs one passing test from a clean checkout

### Phase 1: brute force core (done)

Written test first.

- Bodies stored as structure of arrays
- O(n^2) gravity with Plummer softening
- Leapfrog kick-drift-kick integrator
- Total energy, momentum and angular momentum, summed in 64-bit
- A small native CLI in `tools/` that runs a scene and prints energy drift
- Tests:
  - the force between two bodies matches the formula worked by hand, with and without softening
  - two bodies at the same point give zero force and no NaN
  - forces on a pair are equal and opposite
  - the force matches the slope of the potential energy, checked by nudging a body a tiny distance
  - a two-body circular orbit with softening off returns to its start after one period, 2 * pi * sqrt(r^3 / M), where M is the total mass and r the separation
  - energy drift stays bounded: it is no worse after 10 times as many orbits
  - momentum and angular momentum are conserved
  - running forward then flipping the velocities and running the same number of steps returns to the start
  - halving the timestep cuts the energy error by about 4 (leapfrog is second order). Measured on an eccentric orbit with a coarse timestep, because on a circular orbit the error sinks below 32-bit rounding and the ratio disappears
- Done when: all tests pass and the CLI's energy drift for the two-body scene is recorded in `results/`

### Phase 2: Barnes-Hut (done)

- Octree built fresh every step, storing total mass and center of mass per node
- Adjustable theta. theta = 0 must match brute force to within rounding (the sums happen in a different order, so not bit for bit)
- A depth limit, so two bodies at the same point cannot split the tree forever
- Tests:
  - the root node's mass and center of mass equal the totals over all bodies
  - forces match brute force at theta = 0
  - force error against brute force shrinks as theta shrinks
  - the same checks on a tightly clustered scene, not only a uniform one
- A native benchmark harness timing both methods across several N
- Writes `results/benchmarks.json` and a markdown table
- Done when: force error against brute force is measured for each theta, and the table shows where Barnes-Hut starts to win
- Result: at theta 0.5 the two tied at about 1000 bodies in an evenly spread scene, and the tree was 10.69x faster at 30,000 with 0.59% mean force error. In phase 5 brute force got SIMD and about 2.2x faster, so now the tree first wins between 3000 and 10,000 bodies and is 4.91x faster at 30,000. Tables in `results/benchmarks.md`
- Added while building it: a body never treats a cube on its own path down the tree as one far away body, and each cube's mass is summed from its children so the totals stay exact in 32-bit

### Phase 3: browser (done)

- Install emsdk, add the Emscripten build and bindings
- Vite + TypeScript app in `web/`
- three.js points renderer reading typed array views into WASM memory, no per-frame copy
- Fixed physics timestep decoupled from rendering, with an accumulator and a cap on steps per frame so a slow frame cannot snowball
- Tests stay native. The WASM build runs the same core code but has no test runner of its own
- fps counter
- Open questions to settle here:
  - with structure of arrays, the renderer needs x, y and z as three separate attributes and a small custom shader. three.js cannot work out a bounding box from that, so culling has to be off and the draw count set by hand
  - anything that moves the arrays breaks old views: WASM memory growth, or a vector reallocating when N changes. Plan: allocate for the largest N once and never grow
- Done when: the dev server shows bodies orbiting, with fps measured and recorded
- Result: a disk of 2000 stars runs at 60 fps in Chrome with physics at a median 3.42 ms per step. Numbers in `results/browser.md`
- Settled: the WASM memory is a fixed 64 MB that never grows, and the page grabs new views of x, y and z after anything that changes the body count, instead of reserving a fixed capacity. The compiled `sim.js` and `sim.wasm` are committed, so the web app builds without emsdk
- Added while building it: a disk galaxy scene, needed early because a ball at rest just collapses. A cold disk with a tenth of the center's mass broke into clumps within one turn. That is real physics (the Toomre instability), checked with brute force and a smaller timestep, so the browser disk is light, 0.005 of the center. Numbers in `results/disk-stability.md`

### Phase 4: scenes and controls (done)

- Presets: mini solar system, figure-eight three-body orbit, galaxy collision, black hole against a star cluster
- React control panel: pause, speed, theta and N sliders
- The speed slider changes how many physics steps run per frame. It never changes the timestep
- Click to spawn a body. Spawning adds energy, so it resets the baseline the drift readout compares against
- Live energy drift readout. Exact potential energy is O(n^2), so it is computed every so often and not every frame. Drift is shown relative to the starting energy, which needs care for scenes whose total energy is near zero
- Test: the figure-eight orbit returns close to its start after one period
- Done when: all four presets run and each has its N, timestep and softening written down
- Result: the four presets plus the single disk from phase 3. Settings and energy drift for each are in `results/presets.md`. The worst drift is 1.4e-03, for the black hole and cluster at 1000 stars, and that comes from the tree (at 2000 stars to t = 3.75 it is -9.5e-04 at theta 0.7 and -9.6e-07 at theta 0)
- Settled: the energy sum runs a few rows per frame on a frozen copy of the bodies. The step cap became a 12 ms time budget per frame, since a fixed step count does not fit a speed slider. A spawned body starts on a circular orbit around everything
- Found while building it: the galaxy collision needs softening 0.05, not 0.01, or stars flung through the other galaxy's center wreck the energy (9% jump). The solar system's Jupiter had to move out to 3.0, because at 2.2 it pulled its neighbor 1.4% off its circle

### Phase 5: performance (done)

- WASM SIMD build
- pthreads build, with COOP and COEP headers in `netlify.toml` (now `vercel.json`)
- Fall back to the single thread SIMD build when the page is not cross-origin isolated
- Measure each build against the plain one at the same N, timestep and theta
- Open questions: confirm WebGL accepts views into shared memory in every browser I support, Safari included. The main thread is not allowed to block, so the worker threads have to be created up front
- SIMD should help the brute force loop most. I expect less from it in the tree walk, and will report whatever I measure
- Done when: the speedups are in `results/` with the browser and version they were measured in
- Result: in Chrome 155, SIMD makes brute force 2.44x faster and leaves the tree walk unchanged, and 4 threads make the tree 3.26x faster at 10,000 bodies (2.09x to 2.72x on the scenes of 1000 to 2000 bodies). Numbers in `results/speedups.md`
- Found while building it: SIMD did nothing at first. The compiler would not reorder the brute force sum, and a one line pragma fixed that. Native brute force got about 2.2x faster too, so I reran the Barnes-Hut benchmark and the tree's lead shrank
- Still open: Safari, Firefox and phones are not measured, and I have not confirmed WebGL draws from views into shared memory in Safari. One reading of the live page showed no clear gain from threads, which I have not explained yet

### Phase 6: polish and ship (in progress)

- UI pass (first round done)
- README with gifs and the numbers table
- Vercel deploy
- Final project write-up
- Done so far (commit 7348757):
  - the page downloads only the wasm build it will run: threads when cross-origin isolated, otherwise simd, otherwise the plain build if the browser has no simd
  - a loading message, and an error message if the physics fails to load
  - panel header with the title, a link to the code and a hide button. Space pauses. The panel scrolls on short screens
  - phones (coarse pointer) start with the panel folded and half the default stars, as the devices decision says
  - the camera backs off on tall narrow screens so the whole scene fits across
  - checked in Chrome on Windows (16 threads, 180 Hz screen): galaxy collision at 1x to 4x runs at 90 to 180 fps with no console errors, and the folded panel and phone width layout look right
- Found while doing it: at 8x speed on the galaxy collision (2002 bodies, 16 threads) the page dropped to about 12 fps, with most frames taking 11 ms and about one a second taking around 1 second
- Settled: that stall was Chrome, not the sim. Chrome slows animation frames to about one a second in a window that is not in front, and the first reading was most likely taken in one, since it shows the same pattern. Checked in a background window: frames came about 1005 ms apart at 1x, 4x, 8x and even paused, while timers in the same tab still fired about 100 times a second and each frame's own work took about 13 ms (12.7 ms physics, under 1 ms for the energy sum, 0.2 ms to draw). The same scene at 8x in a window in front runs at about 80 fps
- Next:
  1. ~~diagnose the 8x stall above~~ (settled above)
  2. (skipped for now) record gifs of the galaxy collision, the black hole and the disk. Plan: record the canvas in the page with `MediaRecorder` on `canvas.captureStream(30)`, then turn the webm into a small gif with ffmpeg
  3. README: add the gifs, list the new panel features, keep the numbers tables
  4. ~~write-up in `docs/write-up.md`~~ (done): what it is, the decisions table in short, the measured numbers, what went wrong and how it got fixed (Toomre clumps, galaxy softening, the SIMD pragma, the tree's lead shrinking)
  5. Vercel deploy (switched from Netlify, `vercel.json` sends the same two headers), then update the README with the live link
- Building on Windows: MSVC rejects `-Wextra`, so build the native code in WSL (Ubuntu with `g++`, `cmake` and `make`) or on the Mac. gcc warns that it ignores `#pragma clang loop`, which is expected. All 44 tests pass with g++ 15

## Known risks

- A 32-bit position stops changing when the step `v * dt` is smaller than the gap between neighboring floats at that position. Slow bodies far from the origin can stall. Scenes need to stay near the origin
- "No copy" only covers the JavaScript side. The positions are still uploaded to the GPU each frame

## Numbers

I only report numbers I measured. Each one goes in `results/` with the machine, N, timestep, theta and build flags.
