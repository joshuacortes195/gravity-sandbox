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
  // only for scenes with a black hole (always body 0): the speed of light used to draw its shadow and lensing
  // the physics never uses it, gravity stays newtonian
  lightSpeed?: number;
  // how far above the orbit plane the camera starts, in degrees, when the scene wants something other than the usual
  tilt?: number;
}

export const PRESETS: Preset[] = [
  { name: 'Solar system', dt: 0.01, eps: 0.001, n: null, nMax: 0, camera: 7, dot: 40, spawnMass: 0.001 },
  { name: 'Figure-eight', dt: 0.005, eps: 0, n: null, nMax: 0, camera: 3, dot: 15, spawnMass: 0.3 },
  // n here is stars per galaxy, so the scene has twice that plus the two centers
  { name: 'Galaxy collision', dt: 0.002, eps: 0.05, n: 1000, nMax: 5000, camera: 6, dot: 8, spawnMass: 0.3 },
  // c = 8 puts the horizon at 2M / c^2 = 0.031, big enough to see, inside the roughly 0.05 the cluster's closest stars get to
  // the camera starts low beside the orbit plane, so the drawn disk crosses in front of the shadow
  { name: 'Black hole vs cluster', dt: 0.002, eps: 0.01, n: 1000, nMax: 5000, camera: 3.5, dot: 6, spawnMass: 0.3, lightSpeed: 8, tilt: 6 },
  { name: 'Disk galaxy', dt: 0.002, eps: 0.01, n: 2000, nMax: 10000, camera: 2.7, dot: 6, spawnMass: 0.1 },
];

// the scene that loads first, and the tree accuracy the page starts with
export const START_PRESET = 2;
export const START_THETA = 0.7;

// true on phones and tablets, anything where the main pointer is a finger
export const TOUCH = matchMedia('(pointer: coarse)').matches;

// which looks are on when the page opens, glow starts off on phones since it costs gpu time
// trails start off because they smear the whole picture while the camera turns
// lensing only shows up on scenes with a black hole
export const START_EFFECTS = { glow: !TOUCH, trails: false, speedColors: true, lensing: true };

// starting body count for a scene, halved on touch devices since phones have far less to spend on physics
export function defaultN(preset: number) {
  const n = PRESETS[preset].n;
  if (n === null) return 0;
  return TOUCH ? Math.max(100, Math.round(n / 200) * 100) : n;
}
