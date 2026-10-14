# Browser frame rate

The web app at commit 8db452b, production build (`npm run build`, served with `vite preview`), read from the stats readout in the corner of the page.

- Machine: Intel Core i5-8259U @ 2.30GHz, 4 physical / 8 logical cores, 8 GB RAM, macOS 15.7.7
- GPU: Intel Iris Plus Graphics 655 (WebGL2 through ANGLE on Metal)
- Browser: Chrome 155.0.8059.79, an automated visible window, 1200 x 610 pixels at device pixel ratio 2
- WASM: emcc 6.0.12, -O3, 64 MB fixed memory, one thread, no SIMD
- Scene: disk galaxy, 2000 stars plus the center (2001 bodies), center mass 1, disk mass 0.005, radius 1
- Physics: Barnes-Hut at theta 0.7, softening 0.01, dt 0.002, 120 steps a second, at most 8 steps a frame
- "ms/step" is the time spent inside the WASM step call, averaged over each half second window
- Read every 2 seconds for 30 seconds, 15 readings

## Result

| | Lowest | Median | Highest |
| --- | --- | --- | --- |
| fps | 59.9 | 60.0 | 60.1 |
| physics ms/step | 3.29 | 3.42 | 4.48 |
| steps/s | 120 | 120 | 120 |

The disk stays a disk for at least the first turn of its outer edge, about 26 seconds here. That is as far as I checked it, in `disk-stability.md`. A cold disk is unstable at any mass, so a light one only breaks up slowly, not never.

The screen refreshes at 60 Hz, so 60 fps is the ceiling. This shows the sim keeps up at 2000 bodies. It does not show how much room is left.

## Every reading

| Seconds after load | fps | ms/step | steps/s |
| --- | --- | --- | --- |
| 2 | 60.1 | 4.48 | 120 |
| 4 | 60.0 | 3.46 | 120 |
| 6 | 60.0 | 3.33 | 120 |
| 8 | 59.9 | 3.30 | 120 |
| 10 | 60.0 | 3.30 | 120 |
| 12 | 60.0 | 3.32 | 120 |
| 14 | 60.0 | 3.29 | 120 |
| 16 | 60.0 | 3.41 | 120 |
| 18 | 60.0 | 3.49 | 120 |
| 20 | 60.0 | 3.40 | 120 |
| 22 | 60.0 | 3.42 | 120 |
| 24 | 60.0 | 3.42 | 120 |
| 26 | 60.1 | 3.44 | 120 |
| 28 | 60.1 | 4.38 | 120 |
| 30 | 60.0 | 4.16 | 120 |

## Phase 4: every scene

Same machine, browser and window as above, production build of the phase 4 web app. The step cap is now a 12 ms physics budget per frame instead of 8 steps. One reading per scene, taken about 10 seconds after picking it, at theta 0.7 and 1x speed, so these are single snapshots and not medians. Dt and softening per scene are in [presets.md](presets.md).

Measured on the build just before commit `PHASE4_WEB`. The only changes made after measuring were the figure-eight's dot size, the drift readout waiting for a second energy sum, and the theta slider's lower end. None of them touches the physics.

| Scene | Bodies | fps | ms/step | steps/s | Energy drift |
| --- | --- | --- | --- | --- | --- |
| Solar system | 6 | 60 | 0.01 | 120 | -1.7e-06 |
| Figure-eight | 3 | 60 | 0.00 | 120 | -2.3e-06 |
| Galaxy collision, 1000 stars per disk | 2002 | 60 | 4.89 | 120 | 3.9e-05 |
| Black hole vs cluster, 1000 stars | 1001 | 60 | 3.20 | 120 | -9.4e-04 |
| Disk galaxy, 2000 stars | 2001 | 60 | 3.78 | 120 | 2.4e-06 |

Disk galaxy with the theta and star sliders moved:

| Stars | theta | fps | ms/step |
| --- | --- | --- | --- |
| 2000 | 0 | 6 | 164 |
| 2000 | 1.2 | 60 | 2.39 |
| 10,000 | 0.7 | 41 | 24.1 |
| 10,000 | 1.2 | 48 to 54 | 10.9 |
| 10,000 | 0 | 0 | 4527 |

At theta 0 the tree opens every cube, which is brute force with extra bookkeeping. One step at 10,000 stars took 4.5 seconds and froze the page, because the frame budget always lets at least one step run. So the slider now stops at 0.2. Exact forces are still checked natively in the tests.

At 8x speed the galaxy collision ran 120 to 180 steps a second at 59 to 60 fps, barely faster than 1x. At about 4.9 ms a step only two or three steps fit in the 12 ms budget, so the budget, not the slider, sets the pace. The solar system at 8x does reach 960 steps a second.
