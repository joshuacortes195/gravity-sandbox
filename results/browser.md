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
