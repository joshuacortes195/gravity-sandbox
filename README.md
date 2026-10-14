# Gravity Sandbox

A gravity N-body simulation that runs in the browser. The physics is written in C++ and compiled to WebAssembly, and three.js draws the bodies.

This is a work in progress. The physics core works natively: brute force gravity with softening, a leapfrog integrator, and energy and momentum checks, with 16 tests. There is no browser build or live demo yet.

## Numbers so far

On a two-body orbit run for 1000 orbits (a million steps), total energy stays within about 2 parts in 10,000 of where it started. Halving the timestep cuts the energy error by 3.99, which matches what a second order integrator should do. The full table and how I measured it are in [results/energy-drift.md](results/energy-drift.md).

## What it will do

- Simulate thousands of stars, planets and black holes pulling on each other
- Ship with four scenes: a mini solar system, the figure-eight three-body orbit, a galaxy collision, and a black hole passing through a star cluster
- Show a live energy drift readout so you can see how accurate the simulation is while it runs

The full plan is in [docs/plan.md](docs/plan.md). My notes on the physics are in [docs/physics-notes.md](docs/physics-notes.md).

## Build and test

You need CMake 3.20 or newer and a C++17 compiler.

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

## Layout

| Folder | What is in it |
| --- | --- |
| `core/` | The C++ physics core |
| `tests/` | Native tests, written with doctest |
| `tools/` | `run_scene`, a small CLI that runs a scene and prints energy drift |
| `third_party/` | The doctest header |
| `docs/` | The plan and my physics notes |
| `results/` | Measured numbers and how each one was measured |
