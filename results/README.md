# Results

Every number I quote about this project comes from a file in this folder. Nothing here is estimated.

Each result records how it was measured:

- machine (CPU, cores, RAM, OS)
- compiler and build flags
- N (number of bodies)
- timestep and number of steps
- theta, for Barnes-Hut runs
- softening length
- the commit it was measured on

## Files

| File | What it holds |
| --- | --- |
| [energy-drift.md](energy-drift.md) | Energy and momentum drift for the two-body orbit, and the timestep convergence check |
| [benchmarks.md](benchmarks.md) | Time per force pass for brute force and for Barnes-Hut at four values of theta, from 100 to 30,000 bodies, with the force error of each run |
| [benchmarks.json](benchmarks.json) | The same benchmark numbers as data |
| [disk-stability.md](disk-stability.md) | How long a disk galaxy stays a disk at different disk masses, and why the browser uses 0.005 |
| [browser.md](browser.md) | Frame rate and time per physics step in the browser at 2000 bodies, and the 8x speed stall on Windows |
| [speedups.md](speedups.md) | Time per step for the plain, SIMD and threaded WASM builds in the browser on the Mac and on a 16 thread Windows desktop, and why SIMD did nothing at first |
| [presets.md](presets.md) | Timestep, softening and energy drift for each browser scene, and why the galaxy collision needs more softening |

Both benchmark files are written by `tools/benchmark.cpp`. To measure again:

```bash
# output folder, commit, machine description
./build/benchmark results $(git rev-parse --short HEAD) "my machine"
```

## Machine

All native numbers come from one laptop. Some browser numbers also come from a Windows desktop (AMD Ryzen 7 3700X, 16 threads), and the files that use it say so:

| | |
| --- | --- |
| CPU | Intel Core i5-8259U @ 2.30GHz |
| Cores | 4 physical, 8 logical |
| RAM | 8 GB |
| OS | macOS 15.7.7 |
| Compiler | Apple clang 17.0.0 |
