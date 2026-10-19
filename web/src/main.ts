import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';
import { EffectComposer } from 'three/addons/postprocessing/EffectComposer.js';
import { RenderPass } from 'three/addons/postprocessing/RenderPass.js';
import { AfterimagePass } from 'three/addons/postprocessing/AfterimagePass.js';
import { UnrealBloomPass } from 'three/addons/postprocessing/UnrealBloomPass.js';
import { OutputPass } from 'three/addons/postprocessing/OutputPass.js';
import { createElement } from 'react';
import { createRoot } from 'react-dom/client';
import type { Sim } from './wasm/sim';
import { Panel, type Actions } from './Panel';
import { PRESETS, START_EFFECTS, START_PRESET, START_THETA, defaultN } from './presets';
import { setReadout } from './store';

// physics steps per real second at 1x speed, and the most time one frame may spend on physics
// past that budget the sim slows down instead of the frame rate falling apart
const STEPS_PER_SECOND = 120;
const PHYSICS_BUDGET_MS = 12;

// how many body pairs of the energy sum to add up per frame, and how often a new sum starts
const ENERGY_PAIRS_PER_FRAME = 300_000;
const ENERGY_EVERY_MS = 1000;

// a tiny wasm module that uses one simd instruction, the browser only accepts it if it supports simd
const SIMD_PROBE = new Uint8Array([0, 97, 115, 109, 1, 0, 0, 0, 1, 5, 1, 96, 0, 1, 123, 3, 2, 1, 0, 10, 10, 1, 8, 0, 65, 0, 253, 15, 253, 98, 11]);
const SIMD = WebAssembly.validate(SIMD_PROBE);

// threads need shared memory, which the browser only allows on a cross-origin isolated page
// anywhere else the single thread simd build runs, or the plain build if simd is missing too
const THREADS = crossOriginIsolated && SIMD ? navigator.hardwareConcurrency : 1;
const BUILD = THREADS > 1 ? 'simd + threads' : SIMD ? 'simd' : 'plain';

// only downloads the one build this browser will run
async function loadSim(): Promise<Sim> {
  if (BUILD === 'simd + threads') return (await import('./wasm/sim-mt.js')).default();
  if (BUILD === 'simd') return (await import('./wasm/sim-simd.js')).default();
  return (await import('./wasm/sim.js')).default();
}

// the loading message stays up until the physics is ready, and says so if it never gets there
const status = document.getElementById('status')!;
let sim: Sim;
try {
  sim = await loadSim();
} catch (err) {
  status.textContent = 'the physics failed to load in this browser';
  throw err;
}
status.remove();
sim._threads(THREADS);

// renderer, scene and a camera looking down at the disk from an angle
const renderer = new THREE.WebGLRenderer({ antialias: false });
renderer.setPixelRatio(window.devicePixelRatio);
renderer.setSize(window.innerWidth, window.innerHeight);
document.body.appendChild(renderer.domElement);

const scene = new THREE.Scene();
const camera = new THREE.PerspectiveCamera(50, window.innerWidth / window.innerHeight, 0.01, 200);
camera.up.set(0, 0, 1);

// drag to orbit, scroll or pinch to zoom, works with mouse and touch
const controls = new OrbitControls(camera, renderer.domElement);
controls.enableDamping = true;

// one point per body, the shader builds each position from the three separate x, y, z arrays
const geometry = new THREE.BufferGeometry();
const material = new THREE.ShaderMaterial({
  uniforms: {
    size: { value: 1 },
    total: { value: 1 },
    // a typical speed for the scene, a star moving at it draws white
    typicalSpeed: { value: 1 },
    // 1 colors stars by speed, 0 keeps them all one blue
    speedColors: { value: START_EFFECTS.speedColors ? 1 : 0 },
    color: { value: new THREE.Color(0.55, 0.7, 1.0) },
    slowColor: { value: new THREE.Color(1.0, 0.45, 0.2) },
    midColor: { value: new THREE.Color(1.0, 0.92, 0.82) },
    fastColor: { value: new THREE.Color(0.45, 0.65, 1.0) },
    heavyColor: { value: new THREE.Color(1.0, 0.85, 0.6) },
  },
  vertexShader: /* glsl */ `
    attribute float px;
    attribute float py;
    attribute float pz;
    attribute float vx;
    attribute float vy;
    attribute float vz;
    attribute float m;
    uniform float size;
    uniform float total;
    uniform float typicalSpeed;
    varying float heavy;
    varying float fast;
    void main() {
      vec4 mv = modelViewMatrix * vec4(px, py, pz, 1.0);
      gl_Position = projectionMatrix * mv;
      // share of the total mass, cube rooted so a sun and a planet both stay visible
      heavy = pow(m / total, 1.0 / 3.0);
      // how fast this star goes next to a typical one, 0 at half speed or slower, 0.5 at typical, 1 at double or faster
      fast = clamp(0.5 + 0.5 * log2(length(vec3(vx, vy, vz)) / typicalSpeed), 0.0, 1.0);
      // heavier bodies draw bigger, farther ones smaller, never under a pixel and a half
      gl_PointSize = max(1.5, size * (1.0 + 8.0 * heavy) / -mv.z);
    }
  `,
  fragmentShader: /* glsl */ `
    uniform vec3 color;
    uniform vec3 slowColor;
    uniform vec3 midColor;
    uniform vec3 fastColor;
    uniform vec3 heavyColor;
    uniform float speedColors;
    varying float heavy;
    varying float fast;
    void main() {
      // soft round dot instead of a square
      float a = 1.0 - smoothstep(0.3, 0.5, length(gl_PointCoord - 0.5));
      if (a <= 0.0) discard;
      // slow stars orange, typical ones white, fast ones blue
      vec3 bySpeed = fast < 0.5 ? mix(slowColor, midColor, fast * 2.0) : mix(midColor, fastColor, fast * 2.0 - 1.0);
      vec3 star = mix(color, bySpeed, speedColors);
      // heavy bodies warm white either way
      gl_FragColor = vec4(mix(star, heavyColor, clamp(heavy * 2.0, 0.0, 1.0)) * a, 1.0);
    }
  `,
  // overlapping stars add up and glow
  blending: THREE.AdditiveBlending,
  depthWrite: false,
});
const points = new THREE.Points(geometry, material);

// three.js cannot size up three separate arrays, so skip the on screen check and always draw
points.frustumCulled = false;
scene.add(points);

// faint background stars, only for looks, they are not part of the sim and pull on nothing
const SKY_STARS = 2500;
const skyPositions = new Float32Array(SKY_STARS * 3);
for (let i = 0; i < SKY_STARS; i++) {
  // a random direction, spread evenly over a big sphere
  const z = Math.random() * 2 - 1;
  const angle = Math.random() * Math.PI * 2;
  const r = Math.sqrt(1 - z * z);
  skyPositions.set([100 * r * Math.cos(angle), 100 * r * Math.sin(angle), 100 * z], i * 3);
}
const skyGeometry = new THREE.BufferGeometry();
skyGeometry.setAttribute('position', new THREE.BufferAttribute(skyPositions, 3));
const sky = new THREE.Points(
  skyGeometry,
  // dim enough to stay under the glow threshold, a fixed size so zooming does not change them
  new THREE.PointsMaterial({ color: new THREE.Color(0.35, 0.38, 0.45), size: window.devicePixelRatio, sizeAttenuation: false, depthWrite: false }),
);
scene.add(sky);

// the effects run after the scene is drawn: trails first, so the streaks glow too, then the glow
const composer = new EffectComposer(renderer);
composer.addPass(new RenderPass(scene, camera));
const trails = new AfterimagePass(0.9);
trails.enabled = START_EFFECTS.trails;
composer.addPass(trails);
// strength, how far it spreads, and how bright a spot has to be before it glows
const glow = new UnrealBloomPass(new THREE.Vector2(window.innerWidth, window.innerHeight), 0.8, 0.2, 0.3);
glow.enabled = START_EFFECTS.glow;
composer.addPass(glow);
composer.addPass(new OutputPass());

// points the three attributes straight at the x, y, z arrays inside wasm memory, no copy
// has to run again whenever the body count changes, since the arrays may have moved
function attachViews() {
  // frees the gpu copies of the old arrays before swapping in new ones
  geometry.dispose();
  const n = sim._body_count();
  const buffer = sim.HEAPF32.buffer;
  geometry.setAttribute('px', new THREE.BufferAttribute(new Float32Array(buffer, sim._pos_x(), n), 1));
  geometry.setAttribute('py', new THREE.BufferAttribute(new Float32Array(buffer, sim._pos_y(), n), 1));
  geometry.setAttribute('pz', new THREE.BufferAttribute(new Float32Array(buffer, sim._pos_z(), n), 1));
  geometry.setAttribute('vx', new THREE.BufferAttribute(new Float32Array(buffer, sim._vel_x(), n), 1));
  geometry.setAttribute('vy', new THREE.BufferAttribute(new Float32Array(buffer, sim._vel_y(), n), 1));
  geometry.setAttribute('vz', new THREE.BufferAttribute(new Float32Array(buffer, sim._vel_z(), n), 1));
  const masses = new Float32Array(buffer, sim._mass(), n);
  geometry.setAttribute('m', new THREE.BufferAttribute(masses, 1));
  geometry.setDrawRange(0, n);

  // total mass, for sizing dots and for the speed of spawned bodies
  material.uniforms.total.value = masses.reduce((a, b) => a + b, 0);
}

// the root mean square speed of every body, the white point for the speed colors
// only worked out when a scene loads, so a star's color means the same thing all run long
function typicalSpeed() {
  const n = sim._body_count();
  const buffer = sim.HEAPF32.buffer;
  const vx = new Float32Array(buffer, sim._vel_x(), n);
  const vy = new Float32Array(buffer, sim._vel_y(), n);
  const vz = new Float32Array(buffer, sim._vel_z(), n);
  let sum = 0;
  for (let i = 0; i < n; i++) sum += vx[i] * vx[i] + vy[i] * vy[i] + vz[i] * vz[i];
  return Math.sqrt(sum / Math.max(1, n)) || 1;
}

// what the panel has picked, read by the loop every frame
let preset = PRESETS[START_PRESET];
let theta = START_THETA;
let speed = 1;
let paused = false;
let simTime = 0;

// energy readout: a sum in progress, the energy everything is compared against, and the latest drift
let energyRunning = false;
let energyStarted = 0;
let baseline: number | null = null;
let drift: number | null = null;

// freezes the bodies and starts a fresh energy sum, the loop finishes it a few rows per frame
function startEnergy(now: number) {
  sim._energy_begin();
  energyRunning = true;
  energyStarted = now;
}

// forgets the old baseline, so the next finished sum becomes the new zero for the drift readout
function resetBaseline() {
  baseline = null;
  drift = null;
  startEnergy(performance.now());
}

// builds a preset at the given body count, points the camera at it and restarts the readouts
function load(which: number, n: number) {
  preset = PRESETS[which];
  sim._load_scene(which, n, 1, preset.eps, theta);
  attachViews();
  material.uniforms.typicalSpeed.value = typicalSpeed();
  material.uniforms.size.value = preset.dot * window.devicePixelRatio;
  // tall narrow screens see less side to side, so the camera backs off until the scene fits across
  const back = preset.camera * Math.max(1, 1 / camera.aspect);
  camera.position.set(0, -0.8 * back, 0.6 * back);
  controls.target.set(0, 0, 0);
  simTime = 0;
  resetBaseline();
}

// what the panel is allowed to change
const actions: Actions = {
  load,
  setPaused: (p) => (paused = p),
  setSpeed: (s) => (speed = s),
  setTheta: (t) => (theta = t),
  setGlow: (on) => (glow.enabled = on),
  setTrails: (on) => (trails.enabled = on),
  setSpeedColors: (on) => (material.uniforms.speedColors.value = on ? 1 : 0),
};
load(START_PRESET, defaultN(START_PRESET));
createRoot(document.getElementById('panel')!).render(createElement(Panel, { actions }));

// keeps the picture the right shape when the window changes size
window.addEventListener('resize', () => {
  camera.aspect = window.innerWidth / window.innerHeight;
  camera.updateProjectionMatrix();
  renderer.setSize(window.innerWidth, window.innerHeight);
  composer.setSize(window.innerWidth, window.innerHeight);
});

// a click that did not turn into a drag drops a body where the pointer meets the flat z = 0 plane
const raycaster = new THREE.Raycaster();
const ground = new THREE.Plane(new THREE.Vector3(0, 0, 1), 0);
let downX = 0;
let downY = 0;
renderer.domElement.addEventListener('pointerdown', (e) => {
  if (e.button !== 0) return;
  downX = e.clientX;
  downY = e.clientY;
});
renderer.domElement.addEventListener('pointerup', (e) => {
  // only a plain left click or tap, the other buttons pan the camera
  if (e.button !== 0) return;
  if (Math.hypot(e.clientX - downX, e.clientY - downY) > 5) return;

  // turns the screen point into a ray and finds where it crosses the plane
  const ndc = new THREE.Vector2((e.clientX / window.innerWidth) * 2 - 1, -(e.clientY / window.innerHeight) * 2 + 1);
  raycaster.setFromCamera(ndc, camera);
  const hit = raycaster.ray.intersectPlane(ground, new THREE.Vector3());
  if (!hit) return;

  // starts it on a circular orbit around everything, counterclockwise like the disks, instead of falling straight in
  const r = Math.hypot(hit.x, hit.y);
  const v = r > 0.01 ? Math.sqrt(material.uniforms.total.value / r) : 0;
  const vx = r > 0.01 ? (-v * hit.y) / r : 0;
  const vy = r > 0.01 ? (v * hit.x) / r : 0;
  sim._spawn(hit.x, hit.y, 0, vx, vy, 0, preset.spawnMass, preset.eps, theta);

  // the arrays may have moved and the energy jumped, so new views and a new baseline
  attachViews();
  resetBaseline();
});

// numbers for the readout, reset every half second
let frames = 0;
let stepsDone = 0;
let physicsMs = 0;
let windowStart = performance.now();

// real time that has passed but has not been simulated yet
let backlog = 0;
let last = performance.now();

// runs once per screen refresh: catch the physics up in fixed steps, work on the energy sum, then draw
function frame(now: number) {
  backlog = paused ? 0 : backlog + Math.max(0, now - last) / 1000;
  last = now;

  // as many whole steps as the backlog covers at the chosen speed
  const stepsPerSecond = STEPS_PER_SECOND * speed;
  const steps = Math.floor(backlog * stepsPerSecond);
  backlog -= steps / stepsPerSecond;

  // one step at a time until done or out of time, then drop what is left so a slow frame cannot snowball
  if (steps > 0) {
    const t0 = performance.now();
    let done = 0;
    while (done < steps && performance.now() - t0 < PHYSICS_BUDGET_MS) {
      sim._advance(1, preset.dt, preset.eps, theta);
      done++;
    }
    if (done < steps) backlog = 0;
    physicsMs += performance.now() - t0;
    stepsDone += done;
    simTime += done * preset.dt;

    // tell three.js the positions changed so it sends them to the gpu again
    for (const name of ['px', 'py', 'pz', 'vx', 'vy', 'vz']) geometry.getAttribute(name).needsUpdate = true;
  }

  // a slice of the energy sum, sized so a big scene spreads it over more frames
  if (energyRunning) {
    const rows = Math.max(1, Math.floor(ENERGY_PAIRS_PER_FRAME / sim._body_count()));
    if (sim._energy_work(rows, preset.eps)) {
      const e = sim._energy_value();
      // the first sum after a reset only sets the baseline, so the readout keeps saying measuring
      if (baseline === null) baseline = e;
      else drift = (e - baseline) / Math.abs(baseline);
      energyRunning = false;
    }
  } else if (now - energyStarted >= ENERGY_EVERY_MS) {
    startEnergy(now);
  }

  controls.update();
  // the background stars ride along with the camera so they always look infinitely far away
  sky.position.copy(camera.position);
  // straight to the screen when every effect is off, through the effects otherwise
  if (glow.enabled || trails.enabled) composer.render();
  else renderer.render(scene, camera);

  // sends fresh numbers to the panel twice a second
  frames++;
  if (now - windowStart >= 500) {
    const seconds = (now - windowStart) / 1000;
    setReadout({
      bodies: sim._body_count(),
      fps: frames / seconds,
      msPerStep: stepsDone ? physicsMs / stepsDone : 0,
      stepsPerSecond: stepsDone / seconds,
      simTime,
      drift,
      threads: THREADS,
      build: BUILD,
    });
    frames = 0;
    stepsDone = 0;
    physicsMs = 0;
    windowStart = now;
  }

  requestAnimationFrame(frame);
}
requestAnimationFrame(frame);
