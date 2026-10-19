# Gravity Sandbox

A gravity N-body simulation that runs in the browser. The physics is written in C++ and compiled to WebAssembly, and three.js draws the bodies.

**Live demo: [gravity-sandbox-teal.vercel.app](https://gravity-sandbox-teal.vercel.app/)**

The physics core has brute force gravity with softening, a Barnes-Hut octree for large scenes, a leapfrog integrator, and energy and momentum checks, with 44 tests. The browser build uses SIMD and splits the tree walk across threads when the page allows it. In the browser you can pick one of five scenes, change the speed, the tree accuracy and the number of stars, click to drop in a new body, and watch a live energy drift readout.

How it was built, what I measured and what went wrong along the way is in [docs/write-up.md](docs/write-up.md).

## Numbers so far

On a two-body orbit run for 1000 orbits (a million steps), total energy stays within about 2 parts in 10,000 of where it started. Halving the timestep cuts the energy error by 3.99, which matches what a second order integrator should do. The full table and how I measured it are in [results/energy-drift.md](results/energy-drift.md).

Barnes-Hut against brute force, one force pass on one thread, theta 0.5, bodies spread evenly through a ball:

| Bodies | Brute force | Barnes-Hut | Speedup | Mean force error |
| --- | --- | --- | --- | --- |
| 1000 | 1.499 ms | 3.704 ms | 0.40x | 0.65% |
| 3000 | 14.093 ms | 17.104 ms | 0.82x | 0.68% |
| 10,000 | 164.945 ms | 76.207 ms | 2.16x | 0.61% |
| 30,000 | 1527.296 ms | 311.048 ms | 4.91x | 0.59% |

The tree pulls ahead somewhere between 3000 and 10,000 bodies. Brute force here is the SIMD version, which does 4 bodies per instruction. Other values of theta, a clustered scene and the worst-body errors are in [results/benchmarks.md](results/benchmarks.md).

In the browser (Chrome 155 on the same Intel Mac), with the phase 3 build (one thread, no SIMD), a disk of 2000 stars runs at 60 fps, the screen's limit, with 120 physics steps a second at a median 3.42 ms each, using Barnes-Hut at theta 0.7. Details in [results/browser.md](results/browser.md).

SIMD makes brute force 2.44x faster in Chrome (18.71 to 7.66 ms per step at 2001 bodies), and 4 threads make the tree 3.26x faster on a 10,000 star disk (24.55 to 7.54 ms per step). SIMD does nothing for the tree walk. Details in [results/speedups.md](results/speedups.md).

Each scene's timestep and softening were picked by running it natively and watching the energy. Over the runs in [results/presets.md](results/presets.md) the worst energy drift was:

| Scene | Bodies | Worst energy drift |
| --- | --- | --- |
| Solar system | 6 | 6.7e-06 over two orbits of the outer planet |
| Figure-eight | 3 | 5.3e-06 over ten periods |
| Galaxy collision | 2002 | 6.7e-04 through the crash, to t = 15 |
| Black hole vs cluster | 1001 | 1.4e-03 through the first close pass, to t = 10 |
| Disk galaxy | 2001 | 1.5e-05 to t = 10 |

## What it does

- Simulates thousands of stars, planets and black holes pulling on each other
- Comes with five scenes: a mini solar system, the figure-eight three-body orbit, a galaxy collision, a black hole tearing apart a star cluster, and a single disk galaxy
- Has a control panel for pause, speed, tree accuracy (theta) and the number of stars. Space pauses, and the panel folds away
- Picks the fastest build the browser allows (threads, then SIMD, then plain) and only downloads that one
- Works on phones with touch controls, fewer stars and the panel folded up to start
- Lets you click anywhere to drop in a new body on a circular orbit
- Shows a live energy drift readout so you can see how accurate the simulation is while it runs
- Colors each star by its speed (slow orange, typical white, fast blue), with an optional glow, motion trails and faint background stars. Each look can be switched off in the panel, and glow starts off on phones

The full plan is in [docs/plan.md](docs/plan.md). My notes on the physics are in [docs/physics-notes.md](docs/physics-notes.md).

## Build and test

You need CMake 3.20 or newer and a C++17 compiler (clang or gcc). On Windows, build in WSL, since MSVC does not take the `-Wextra` flag.

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

To run the two-body orbit and print its energy drift:

```bash
# speed scale (1 is circular), steps per orbit, orbits
./build/run_scene 1 1000 100
```

To run one of the browser's scenes natively and print its energy drift:

```bash
# scene (solar, eight, collision, cluster, disk), stars, dt, softening, theta, time, checkpoints
./build/preset_check collision 1000 0.002 0.05 0.7 15 6
```

To time brute force against Barnes-Hut (takes about a minute and rewrites the two benchmark files):

```bash
# output folder, commit, machine description
./build/benchmark results $(git rev-parse --short HEAD) "my machine"
```

## Run it in the browser

You need [emsdk](https://emscripten.org/docs/getting_started/downloads.html) and Node. The compiled `sim.js` and `sim.wasm` are committed in `web/src/wasm/`, so the web app runs without emsdk unless you change the C++.

```bash
# rebuild the wasm module after changing core/ or wasm/
source ~/emsdk/emsdk_env.sh
emcmake cmake -S . -B build-wasm
cmake --build build-wasm

# start the dev server
cd web
npm install
npm run dev
```

## Layout

| Folder | What is in it |
| --- | --- |
| `core/` | The C++ physics core |
| `tests/` | Native tests, written with doctest |
| `tools/` | `run_scene`, a small CLI that runs the two-body orbit and prints energy drift, `benchmark`, which times brute force against Barnes-Hut, `disk_check` for disk stability and `preset_check` for each browser scene's energy drift |
| `wasm/` | The functions the web page calls into the C++ core |
| `web/` | The Vite + TypeScript app that draws the sim with three.js |
| `third_party/` | The doctest header |
| `docs/` | The plan and my physics notes |
| `results/` | Measured numbers and how each one was measured |
