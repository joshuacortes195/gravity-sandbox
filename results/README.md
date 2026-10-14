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

Benchmarks come in phase 2.

## Machine

All native numbers come from one laptop:

| | |
| --- | --- |
| CPU | Intel Core i5-8259U @ 2.30GHz |
| Cores | 4 physical, 8 logical |
| RAM | 8 GB |
| OS | macOS 15.7.7 |
| Compiler | Apple clang 17.0.0 |
