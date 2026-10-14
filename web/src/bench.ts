import createPlain from './wasm/sim.js';
import createSimd from './wasm/sim-simd.js';
import createMt from './wasm/sim-mt.js';
import type { Sim } from './wasm/sim';
import { PRESETS } from './presets';

// times every build of the physics on the same scenes, so the speedups come from one page in one browser
// open /bench.html, it prints a table and leaves the raw numbers on window.bench

// how long steps run before timing starts, and how the timed steps are grouped
// safari first runs wasm through a quick compile and swaps in the fast one later, so the warmup goes by time
const WARMUP_MS = 1000;
const BATCHES = 15;
const STEPS_PER_BATCH = 5;
const THETA = 0.7;

// the scenes, all at their browser settings: which preset, how many bodies, tree or brute force
const CASES = [
  { label: 'disk 2000, tree', which: 4, n: 2000, brute: false },
  { label: 'disk 10000, tree', which: 4, n: 10000, brute: false },
  { label: 'collision 2x1000, tree', which: 2, n: 1000, brute: false },
  { label: 'cluster 1000, tree', which: 3, n: 1000, brute: false },
  { label: 'disk 2000, brute force', which: 4, n: 2000, brute: true },
];

interface Row {
  build: string;
  threads: number;
  scene: string;
  bodies: number;
  msPerStep: number;
}

const out = document.getElementById('out')!;
const rows: Row[] = [];

// gives the page a moment to breathe between runs, so the text updates and finished workers get handed back
const pause = () => new Promise((r) => setTimeout(r, 50));

// middle value of a list
function median(xs: number[]) {
  const s = [...xs].sort((a, b) => a - b);
  return s[Math.floor(s.length / 2)];
}

// loads one scene into one build, warms it up, then times batches of steps and keeps the median
function time(sim: Sim, c: (typeof CASES)[number]) {
  const p = PRESETS[c.which];
  sim._load_scene(c.which, c.n, 1, p.eps, THETA);
  const run = (steps: number) =>
    c.brute ? sim._advance_brute(steps, p.dt, p.eps) : sim._advance(steps, p.dt, p.eps, THETA);
  for (const t0 = performance.now(); performance.now() - t0 < WARMUP_MS; ) run(1);
  const perStep: number[] = [];
  for (let b = 0; b < BATCHES; b++) {
    const t0 = performance.now();
    run(STEPS_PER_BATCH);
    perStep.push((performance.now() - t0) / STEPS_PER_BATCH);
  }
  return { bodies: sim._body_count(), ms: median(perStep) };
}

// the table so far, one line per run
function show(status: string) {
  const lines = rows.map(
    (r) =>
      `${r.build.padEnd(5)} ${String(r.threads).padStart(2)}  ${r.scene.padEnd(24)} ${String(r.bodies).padStart(6)}  ${r.msPerStep.toFixed(2).padStart(8)} ms/step`,
  );
  out.textContent = [
    `${navigator.userAgent}`,
    `cross-origin isolated: ${crossOriginIsolated}, logical cores: ${navigator.hardwareConcurrency}`,
    `theta ${THETA}, median of ${BATCHES} batches of ${STEPS_PER_BATCH} steps after ${WARMUP_MS} ms of warmup`,
    '',
    ...lines,
    '',
    status,
  ].join('\n');
}

// thread counts to try on the threaded build: 1, 2, 4, ... up to the core count
const threadCounts: number[] = [];
for (let k = 1; k <= navigator.hardwareConcurrency; k *= 2) threadCounts.push(k);

// every build on every scene, the threaded one at each thread count (brute force is not threaded)
const builds: [string, () => Promise<Sim>][] = [
  ['plain', createPlain],
  ['simd', createSimd],
];
if (crossOriginIsolated) builds.push(['mt', createMt]);

for (const [name, create] of builds) {
  const sim = await create();
  for (const c of CASES) {
    for (const k of name === 'mt' && !c.brute ? threadCounts : [1]) {
      show(`running ${name} x${k} on ${c.label}...`);
      // stops the old helpers and lets their workers go back to the pool before asking for new ones
      if (name === 'mt') sim._threads(1);
      await pause();
      if (name === 'mt') sim._threads(k);
      const { bodies, ms } = time(sim, c);
      rows.push({ build: name, threads: k, scene: c.label, bodies, msPerStep: ms });
    }
  }
  if (name === 'mt') sim._threads(1);
}

show('done');
// raw numbers for copying into results/
(window as unknown as { bench: unknown }).bench = {
  userAgent: navigator.userAgent,
  crossOriginIsolated,
  cores: navigator.hardwareConcurrency,
  theta: THETA,
  rows,
};
