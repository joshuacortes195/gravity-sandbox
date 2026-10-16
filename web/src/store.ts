// the numbers the panel shows, written by the render loop twice a second and read by react
export interface Readout {
  bodies: number;
  fps: number;
  msPerStep: number;
  stepsPerSecond: number;
  simTime: number;
  drift: number | null; // energy change since the baseline, null until the first sum finishes
  threads: number; // how many threads share the force pass
  build: string; // which wasm build is running: plain, simd, or simd + threads
}

let readout: Readout = { bodies: 0, fps: 0, msPerStep: 0, stepsPerSecond: 0, simTime: 0, drift: null, threads: 1, build: '' };
const listeners = new Set<() => void>();

// swaps in new numbers and tells every subscribed component
export function setReadout(next: Readout) {
  readout = next;
  for (const l of listeners) l();
}

// hooks for useSyncExternalStore
export function subscribe(l: () => void) {
  listeners.add(l);
  return () => listeners.delete(l);
}
export const getReadout = () => readout;
