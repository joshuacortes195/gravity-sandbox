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

## Windows desktop, up to 16 threads

The same benchmark page, on a second machine with more cores, to see whether the page should use every core or stop at 4.

- Machine: AMD Ryzen 7 3700X, 8 physical / 16 logical cores, 16 GB RAM, NVIDIA GeForce RTX 3060, Windows 10 Home
- Commit: 597f2a9, the live site (https://gravity-sandbox-teal.vercel.app/bench.html), cross-origin isolated
- WASM: the same three builds and flags as above, rebuilt on Windows with emcc 6.0.12
- Browser: Chrome 156, an automated window that was not in front. The benchmark times its steps with timers, not animation frames, so the frame slowdown Chrome puts on a background window (see [browser.md](browser.md)) does not apply to it
- `navigator.hardwareConcurrency` reported 16, so the threaded build was tried at 1, 2, 4, 8 and 16 threads
- Each number is the median of 3 page loads, the range is the lowest and highest. The third load was slower on every row, by up to about 1.8x, while the machine was short on memory. It moves the highest values, not the medians

| Scene | Bodies | plain | simd | mt, 4 threads | mt, 8 threads | mt, 16 threads |
| --- | --- | --- | --- | --- | --- | --- |
| disk, tree | 2001 | 2.82 ms | 3.16 ms (0.89x) | 1.23 ms (2.29x) | 0.99 ms (2.85x) | 0.94 ms (3.00x) |
| disk, tree | 10001 | 19.37 ms | 19.62 ms (0.99x) | 6.98 ms (2.78x) | 4.78 ms (4.05x) | 4.03 ms (4.81x) |
| galaxy collision, tree | 2002 | 3.67 ms | 3.96 ms (0.93x) | 1.73 ms (2.12x) | 1.18 ms (3.11x) | 0.98 ms (3.74x) |
| black hole and cluster, tree | 1001 | 3.15 ms | 3.10 ms (1.02x) | 1.08 ms (2.92x) | 0.65 ms (4.85x) | 0.62 ms (5.08x) |
| disk, brute force | 2001 | 14.11 ms | 5.50 ms (2.57x) | not threaded | not threaded | not threaded |

What I take from it:

- 16 threads is the fastest on every scene, so the page keeps using every core it is given on desktops. Past 8 the gain is small (5% to 19%), since 8 of the 16 are the second hardware thread on a core that is already busy
- SIMD again speeds up brute force (2.57x) and does nothing for the tree, the same as on the Mac
- The threaded build on 1 thread: disk 2001 2.91 ms, disk 10001 19.63 ms, collision 3.88 ms, cluster 3.04 ms

### Every run

ms per step, runs 1 / 2 / 3. Plain and simd are 1 thread.

| Build | Threads | Scene | Bodies | Run 1 | Run 2 | Run 3 |
| --- | --- | --- | --- | --- | --- | --- |
| plain | 1 | disk, tree | 2001 | 2.95 | 2.77 | 2.82 |
| plain | 1 | disk, tree | 10001 | 19.73 | 19.37 | 18.14 |
| plain | 1 | collision, tree | 2002 | 3.67 | 3.68 | 3.62 |
| plain | 1 | cluster, tree | 1001 | 2.98 | 3.15 | 3.49 |
| plain | 1 | disk, brute force | 2001 | 14.10 | 14.11 | 16.74 |
| simd | 1 | disk, tree | 2001 | 3.16 | 2.93 | 3.36 |
| simd | 1 | disk, tree | 10001 | 19.62 | 19.34 | 22.41 |
| simd | 1 | collision, tree | 2002 | 3.73 | 3.96 | 4.48 |
| simd | 1 | cluster, tree | 1001 | 2.84 | 3.10 | 3.69 |
| simd | 1 | disk, brute force | 2001 | 5.24 | 5.50 | 7.64 |
| mt | 1 | disk, tree | 2001 | 2.86 | 2.91 | 4.03 |
| mt | 2 | disk, tree | 2001 | 1.66 | 1.64 | 2.56 |
| mt | 4 | disk, tree | 2001 | 1.23 | 1.21 | 1.66 |
| mt | 8 | disk, tree | 2001 | 0.97 | 0.99 | 1.76 |
| mt | 16 | disk, tree | 2001 | 0.87 | 0.94 | 1.14 |
| mt | 1 | disk, tree | 10001 | 19.63 | 19.25 | 25.57 |
| mt | 2 | disk, tree | 10001 | 9.52 | 11.26 | 15.97 |
| mt | 4 | disk, tree | 10001 | 6.48 | 6.98 | 10.31 |
| mt | 8 | disk, tree | 10001 | 4.77 | 4.78 | 7.20 |
| mt | 16 | disk, tree | 10001 | 4.00 | 4.03 | 7.10 |
| mt | 1 | collision, tree | 2002 | 3.88 | 3.87 | 5.68 |
| mt | 2 | collision, tree | 2002 | 2.03 | 2.00 | 2.99 |
| mt | 4 | collision, tree | 2002 | 1.44 | 1.73 | 2.16 |
| mt | 8 | collision, tree | 2002 | 1.18 | 1.03 | 1.74 |
| mt | 16 | collision, tree | 2002 | 0.98 | 0.93 | 1.75 |
| mt | 1 | cluster, tree | 1001 | 3.04 | 2.86 | 5.24 |
| mt | 2 | cluster, tree | 1001 | 1.62 | 1.49 | 2.74 |
| mt | 4 | cluster, tree | 1001 | 1.08 | 0.96 | 1.70 |
| mt | 8 | cluster, tree | 1001 | 0.65 | 0.60 | 1.36 |
| mt | 16 | cluster, tree | 1001 | 0.62 | 0.55 | 1.38 |
| mt | 1 | disk, brute force | 2001 | 5.39 | 5.19 | 8.05 |

### Threads on the live page

The main page on the same machine, galaxy collision at 1x and theta 0.7, read from the panel about a second after load. Single readings, not medians.

| Where | Commit | Build | Threads | ms/step | steps/s |
| --- | --- | --- | --- | --- | --- |
| live site | 597f2a9 | simd + threads | 16 | 0.89 | 119 |
| `vite preview` of the production build | the review fixes, before they were committed | simd + threads | 16 | 1.31 | 119 |

Both are close to the benchmark's 0.98 ms on 16 threads and far under its 3.67 ms plain, so on this machine threads do help the live page. The Mac reading above, where they seemed not to, is still not explained.
