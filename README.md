# Gravity Sandbox

A gravity N-body simulation that runs in the browser. The physics is written in C++ and compiled to WebAssembly, and three.js draws the bodies.

This is a work in progress. Right now the repo has the build setup, the test runner and the plan. There is no physics or live demo yet.

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

## Layout

| Folder | What is in it |
| --- | --- |
| `core/` | The C++ physics core |
| `tests/` | Native tests, written with doctest |
| `third_party/` | The doctest header |
| `docs/` | The plan and my physics notes |
| `results/` | Measured numbers and how each one was measured |
