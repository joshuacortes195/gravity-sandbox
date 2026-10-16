// every scene the page can load, in the same order as load_scene in the bindings
// dt and eps were picked with tools/preset_check, numbers in results/presets.md
export interface Preset {
  name: string;
  dt: number; // the one fixed timestep
  eps: number; // softening length
  n: number | null; // default body count for the slider, null for scenes with a fixed cast
  nMax: number; // top of the body count slider
  camera: number; // how far back the camera starts
  dot: number; // base point size, bigger for scenes with a handful of bodies
  spawnMass: number; // mass of a body dropped in with a click
}

export const PRESETS: Preset[] = [
  { name: 'Solar system', dt: 0.01, eps: 0.001, n: null, nMax: 0, camera: 7, dot: 40, spawnMass: 0.001 },
  { name: 'Figure-eight', dt: 0.005, eps: 0, n: null, nMax: 0, camera: 3, dot: 15, spawnMass: 0.3 },
  // n here is stars per galaxy, so the scene has twice that plus the two centers
  { name: 'Galaxy collision', dt: 0.002, eps: 0.05, n: 1000, nMax: 5000, camera: 6, dot: 8, spawnMass: 0.3 },
  { name: 'Black hole vs cluster', dt: 0.002, eps: 0.01, n: 1000, nMax: 5000, camera: 3.5, dot: 6, spawnMass: 0.3 },
  { name: 'Disk galaxy', dt: 0.002, eps: 0.01, n: 2000, nMax: 10000, camera: 2.7, dot: 6, spawnMass: 0.1 },
];

// the scene that loads first, and the tree accuracy the page starts with
export const START_PRESET = 2;
export const START_THETA = 0.7;

// true on phones and tablets, anything where the main pointer is a finger
export const TOUCH = matchMedia('(pointer: coarse)').matches;

// starting body count for a scene, halved on touch devices since phones have far less to spend on physics
export function defaultN(preset: number) {
  const n = PRESETS[preset].n;
  if (n === null) return 0;
  return TOUCH ? Math.max(100, Math.round(n / 200) * 100) : n;
}
