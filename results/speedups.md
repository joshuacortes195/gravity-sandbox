# SIMD and threads in the browser

Each of the three WASM builds timed on the same scenes, from one page in one browser, so the speedups compare like with like. Written up from `web/bench.html` (`web/src/bench.ts`).

- Machine: Intel Core i5-8259U @ 2.30GHz, 4 physical / 8 logical cores, 8 GB RAM, macOS 15.7.7
- Commit: 8410f78, production build (`npm run build`, served with `vite preview` with the COOP and COEP headers)
- WASM: emcc 6.0.12, Release (-O3 -DNDEBUG), 64 MB fixed memory
  - plain: no extra flags, the baseline
  - simd: `-msimd128`
  - mt: `-msimd128 -pthread`, one worker per reported core started while the page loads
- Browser: Chrome 155.0.8059.41, an automated visible window. The page was cross-origin isolated. `navigator.hardwareConcurrency` reported 4, so the threaded build was tried at 1, 2 and 4 threads. 8 was not tried
- Each scene at its browser preset settings (`web/src/presets.ts`), seed 1, theta 0.7
- Time: one whole step (tree build, tree walk or brute force, and the leapfrog update), the median of 15 batches of 5 steps after 1 second of warmup steps
- Each number below is the median of 3 separate page loads. The range is the lowest and highest of those 3
- Speedup is the plain build's median divided by this one, on the same scene, from the medians as printed

## Result

| Scene | Bodies | plain | simd | mt, 2 threads | mt, 4 threads |
| --- | --- | --- | --- | --- | --- |
| disk, tree | 2001 | 3.53 ms | 3.60 ms (0.98x) | 2.04 ms (1.73x) | 1.30 ms (2.72x) |
| disk, tree | 10001 | 24.55 ms | 24.90 ms (0.99x) | 12.57 ms (1.95x) | 7.54 ms (3.26x) |
| galaxy collision, tree | 2002 | 4.47 ms | 4.36 ms (1.03x) | 2.46 ms (1.82x) | 2.14 ms (2.09x) |
| black hole and cluster, tree | 1001 | 3.74 ms | 3.62 ms (1.03x) | 2.06 ms (1.82x) | 1.54 ms (2.43x) |
| disk, brute force | 2001 | 18.71 ms | 7.66 ms (2.44x) | not threaded | not threaded |

What I take from it:

- SIMD speeds up brute force 2.44x and does nothing for the tree. Brute force is one flat loop the compiler can run 4 bodies at a time. The tree walk branches on every cube
- 4 threads give 3.26x on the big disk, and only 2.09x to 2.72x on the scenes of 1000 to 2000 bodies. The tree build alone cannot explain that: natively it is about 3% of a tree force pass at 1000 to 2000 bodies (`benchmarks.md`), which would still allow about 3.5x. The other costs that do not shrink with threads are the leapfrog update, which stays on one thread, and the fixed cost of waking the helpers and waiting for them every step, with only 4 to 8 blocks of 64 bodies per thread at this size (16 blocks for 1001 bodies, 32 for about 2000). I did not measure how the time splits between those. Two of the 4 thread runs on small scenes were also slow outliers (2.95 and 2.73 ms)
- The threaded build on 1 thread runs at 0.93x to 1.00x of plain on the tree scenes, against 0.98x to 1.03x for simd. Building with threads costs up to about 12% against simd on one thread (cluster: 4.04 ms against 3.62 ms, 8% against plain), and 2 threads already more than make that back

## Every run

ms per step, runs 1 / 2 / 3.

| Build | Threads | Scene | Bodies | Run 1 | Run 2 | Run 3 |
| --- | --- | --- | --- | --- | --- | --- |
| plain | 1 | disk, tree | 2001 | 3.64 | 3.53 | 3.40 |
| plain | 1 | disk, tree | 10001 | 24.55 | 25.48 | 23.55 |
| plain | 1 | collision, tree | 2002 | 4.47 | 4.49 | 4.26 |
| plain | 1 | cluster, tree | 1001 | 3.79 | 3.74 | 3.50 |
| plain | 1 | disk, brute force | 2001 | 18.71 | 18.95 | 18.49 |
| simd | 1 | disk, tree | 2001 | 3.60 | 3.81 | 3.43 |
| simd | 1 | disk, tree | 10001 | 24.77 | 24.95 | 24.90 |
| simd | 1 | collision, tree | 2002 | 4.49 | 4.36 | 4.29 |
| simd | 1 | cluster, tree | 1001 | 3.76 | 3.62 | 3.60 |
| simd | 1 | disk, brute force | 2001 | 7.66 | 7.69 | 7.46 |
| mt | 1 | disk, tree | 2001 | 3.57 | 3.62 | 3.30 |
| mt | 2 | disk, tree | 2001 | 2.08 | 2.04 | 1.97 |
| mt | 4 | disk, tree | 2001 | 1.30 | 1.24 | 1.39 |
| mt | 1 | disk, tree | 10001 | 24.45 | 25.85 | 22.83 |
| mt | 2 | disk, tree | 10001 | 13.00 | 12.57 | 12.20 |
| mt | 4 | disk, tree | 10001 | 7.61 | 7.54 | 7.01 |
| mt | 1 | collision, tree | 2002 | 4.75 | 4.72 | 4.70 |
| mt | 2 | collision, tree | 2002 | 2.57 | 2.46 | 2.45 |
| mt | 4 | collision, tree | 2002 | 2.95 | 2.14 | 1.93 |
| mt | 1 | cluster, tree | 1001 | 4.25 | 4.04 | 3.88 |
| mt | 2 | cluster, tree | 1001 | 2.29 | 2.06 | 1.97 |
| mt | 4 | cluster, tree | 1001 | 2.73 | 1.54 | 1.48 |
| mt | 1 | disk, brute force | 2001 | 7.77 | 7.61 | 7.79 |

## The first run, before the fix

The first three page loads, at commit 7742dab, showed SIMD doing nothing even for brute force: medians of 18.61 ms plain against 18.52 ms simd. Asking the compiler why (`-Rpass-missed=loop-vectorize`) gave "loop not vectorized: cannot prove it is safe to reorder floating-point operations". The loop adds every pull into one running total. Doing 4 bodies at once means keeping 4 totals and adding them at the end, which changes the order of the float adds, and float addition gives slightly different answers in a different order. The compiler will not do that unless told it may. A `#pragma clang loop vectorize(enable)` on that one loop allows it, and the next three loads are the numbers above. All 44 native tests still pass with the reordered sum.

The same pragma also vectorizes the native build, so native brute force got about 2.2 times faster too. `benchmarks.md` was rerun at 8410f78 for that reason.

## Main page

The main page in the same Chrome, cross-origin isolated, picked the threaded build with 4 threads. On the galaxy collision preset (2002 bodies, 1x speed, theta 0.7) about 10 seconds after load the readout showed 60 fps, 3.69 ms per step and 120 steps a second, with no console errors. That was one reading at commit 739ac8c, before the fix (the fix does not touch the tree code the main page uses). Its ms/step includes the time the main thread spins waiting on its helpers. 3.69 ms is well above the 2.14 ms the benchmark page measured for the same scene on 4 threads, and close to one thread (4.36 ms). The main page read it about 10 seconds into the scene while the benchmark times the start, and the main page is also drawing every frame, but I have not checked which of those explains the gap, so this one reading does not show threads helping the live page.

## Not measured

Safari, Firefox and phones. The page asks for the threaded build only when it is cross-origin isolated, so any browser that refuses shared memory gets the single thread SIMD build. I have not checked that Safari draws from views into shared memory.
