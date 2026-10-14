import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';
import { createElement } from 'react';
import { createRoot } from 'react-dom/client';
import createSim from './wasm/sim.js';
import { Panel, type Actions } from './Panel';
import { PRESETS, START_PRESET } from './presets';
import { setReadout } from './store';

// tree accuracy the page starts with
const START_THETA = 0.7;

// physics steps per real second at 1x speed, and the most time one frame may spend on physics
// past that budget the sim slows down instead of the frame rate falling apart
const STEPS_PER_SECOND = 120;
const PHYSICS_BUDGET_MS = 12;

// how many body pairs of the energy sum to add up per frame, and how often a new sum starts
const ENERGY_PAIRS_PER_FRAME = 300_000;
const ENERGY_EVERY_MS = 1000;

const sim = await createSim();

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
    color: { value: new THREE.Color(0.55, 0.7, 1.0) },
    heavyColor: { value: new THREE.Color(1.0, 0.85, 0.6) },
  },
  vertexShader: /* glsl */ `
    attribute float px;
    attribute float py;
    attribute float pz;
    attribute float m;
    uniform float size;
    uniform float total;
    varying float heavy;
    void main() {
      vec4 mv = modelViewMatrix * vec4(px, py, pz, 1.0);
      gl_Position = projectionMatrix * mv;
      // share of the total mass, cube rooted so a sun and a planet both stay visible
      heavy = pow(m / total, 1.0 / 3.0);
      // heavier bodies draw bigger, farther ones smaller, never under a pixel and a half
      gl_PointSize = max(1.5, size * (1.0 + 5.0 * heavy) / -mv.z);
    }
  `,
  fragmentShader: /* glsl */ `
    uniform vec3 color;
    uniform vec3 heavyColor;
    varying float heavy;
    void main() {
      // soft round dot instead of a square
      float a = 1.0 - smoothstep(0.3, 0.5, length(gl_PointCoord - 0.5));
      if (a <= 0.0) discard;
      // light stars blue, heavy bodies warm white
      gl_FragColor = vec4(mix(color, heavyColor, clamp(heavy * 2.0, 0.0, 1.0)) * a, 1.0);
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

// points the three attributes straight at the x, y, z arrays inside wasm memory, no copy
// has to run again whenever the body count changes, since the arrays may have moved
function attachViews() {
  const n = sim._body_count();
  const buffer = sim.HEAPF32.buffer;
  geometry.setAttribute('px', new THREE.BufferAttribute(new Float32Array(buffer, sim._pos_x(), n), 1));
  geometry.setAttribute('py', new THREE.BufferAttribute(new Float32Array(buffer, sim._pos_y(), n), 1));
  geometry.setAttribute('pz', new THREE.BufferAttribute(new Float32Array(buffer, sim._pos_z(), n), 1));
  const masses = new Float32Array(buffer, sim._mass(), n);
  geometry.setAttribute('m', new THREE.BufferAttribute(masses, 1));
  geometry.setDrawRange(0, n);

  // total mass, for sizing dots and for the speed of spawned bodies
  material.uniforms.total.value = masses.reduce((a, b) => a + b, 0);
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
  material.uniforms.size.value = preset.dot * window.devicePixelRatio;
  camera.position.set(0, -0.8 * preset.camera, 0.6 * preset.camera);
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
};
load(START_PRESET, PRESETS[START_PRESET].n ?? 0);
createRoot(document.getElementById('panel')!).render(createElement(Panel, { actions }));

// keeps the picture the right shape when the window changes size
window.addEventListener('resize', () => {
  camera.aspect = window.innerWidth / window.innerHeight;
  camera.updateProjectionMatrix();
  renderer.setSize(window.innerWidth, window.innerHeight);
});

// a click that did not turn into a drag drops a body where the pointer meets the flat z = 0 plane
const raycaster = new THREE.Raycaster();
const ground = new THREE.Plane(new THREE.Vector3(0, 0, 1), 0);
let downX = 0;
let downY = 0;
renderer.domElement.addEventListener('pointerdown', (e) => {
  downX = e.clientX;
  downY = e.clientY;
});
renderer.domElement.addEventListener('pointerup', (e) => {
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
    for (const name of ['px', 'py', 'pz']) geometry.getAttribute(name).needsUpdate = true;
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
  renderer.render(scene, camera);

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
    });
    frames = 0;
    stepsDone = 0;
    physicsMs = 0;
    windowStart = now;
  }

  requestAnimationFrame(frame);
}
requestAnimationFrame(frame);
