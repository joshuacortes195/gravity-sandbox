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

Both benchmark files are written by `tools/benchmark.cpp`. To measure again:

```bash
# output folder, commit, machine description
./build/benchmark results $(git rev-parse --short HEAD) "my machine"
```

## Machine

All native numbers come from one laptop:

| | |
| --- | --- |
| CPU | Intel Core i5-8259U @ 2.30GHz |
| Cores | 4 physical, 8 logical |
| RAM | 8 GB |
| OS | macOS 15.7.7 |
| Compiler | Apple clang 17.0.0 |
