import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';
import { EffectComposer } from 'three/addons/postprocessing/EffectComposer.js';
import { RenderPass } from 'three/addons/postprocessing/RenderPass.js';
import { AfterimagePass } from 'three/addons/postprocessing/AfterimagePass.js';
import { UnrealBloomPass } from 'three/addons/postprocessing/UnrealBloomPass.js';
import { OutputPass } from 'three/addons/postprocessing/OutputPass.js';
import { ShaderPass } from 'three/addons/postprocessing/ShaderPass.js';
import { createElement } from 'react';
import { createRoot } from 'react-dom/client';
import type { Sim } from './wasm/sim';
import { Panel, type Actions } from './Panel';
import { PRESETS, START_EFFECTS, START_PRESET, START_THETA, TOUCH, defaultN } from './presets';
import { setReadout } from './store';

// physics steps per real second at 1x speed, and the most time one frame may spend on physics
// past that budget the sim slows down instead of the frame rate falling apart
const STEPS_PER_SECOND = 120;
const PHYSICS_BUDGET_MS = 12;

// how many body pairs of the energy sum to add up per frame, and how often a new sum starts
const ENERGY_PAIRS_PER_FRAME = 300_000;
const ENERGY_EVERY_MS = 1000;

// how much of a trail is left after one sixtieth of a second, the per frame amount is worked out from this
const TRAIL_KEEP_PER_60TH = 0.9;

// a tiny wasm module that uses one simd instruction, the browser only accepts it if it supports simd
const SIMD_PROBE = new Uint8Array([0, 97, 115, 109, 1, 0, 0, 0, 1, 5, 1, 96, 0, 1, 123, 3, 2, 1, 0, 10, 10, 1, 8, 0, 65, 0, 253, 15, 253, 98, 11]);
const SIMD = WebAssembly.validate(SIMD_PROBE);

// threads need shared memory, which the browser only allows on a cross-origin isolated page
// desktops use every core, on a 16 thread ryzen 16 beat 8 and 4 on every scene (results/speedups.md)
// phones get one thread, as the devices decision in the plan says
const WANT_THREADS = crossOriginIsolated && SIMD && !TOUCH ? navigator.hardwareConcurrency : 1;

// gives up on a load that never finishes, so a stuck threaded build still falls back to the next one
function withTimeout<T>(p: Promise<T>, ms: number) {
  return Promise.race([p, new Promise<never>((_, reject) => setTimeout(() => reject(new Error(`no answer after ${ms} ms`)), ms))]);
}

// tries the best build this browser could run first, and only downloads the next one if that fails
async function loadSim() {
  const tries: { build: string; threads: number; load: () => Promise<Sim> }[] = [];
  if (WANT_THREADS > 1) tries.push({ build: 'simd + threads', threads: WANT_THREADS, load: async () => withTimeout((await import('./wasm/sim-mt.js')).default(), 20_000) });
  if (SIMD) tries.push({ build: 'simd', threads: 1, load: async () => (await import('./wasm/sim-simd.js')).default() });
  tries.push({ build: 'plain', threads: 1, load: async () => (await import('./wasm/sim.js')).default() });

  let lastError: unknown;
  for (const t of tries) {
    try {
      return { sim: await t.load(), build: t.build, threads: t.threads };
    } catch (err) {
      lastError = err;
      console.warn(`the ${t.build} build failed to load, trying the next one`, err);
    }
  }
  throw lastError;
}

// the message in the middle of the screen, used while loading and when something breaks
const status = document.getElementById('status')!;
function showStatus(text: string) {
  status.textContent = text;
  if (!status.isConnected) document.body.appendChild(status);
}

// the loading message stays up until the physics is ready, and says so if it never gets there
let loaded: Awaited<ReturnType<typeof loadSim>>;
try {
  loaded = await loadSim();
} catch (err) {
  showStatus('the physics failed to load in this browser');
  throw err;
}
const { sim, build: BUILD, threads: THREADS } = loaded;
sim._threads(THREADS);

// the screen's pixel density, capped at 2 since a 3x phone screen would draw 9 times the pixels for little gain
const pixelRatio = () => Math.min(window.devicePixelRatio, 2);

// renderer, scene and a camera looking down at the disk from an angle
let renderer: THREE.WebGLRenderer;
try {
  renderer = new THREE.WebGLRenderer({ antialias: false });
} catch (err) {
  showStatus('this browser could not start webgl, which the drawing needs');
  throw err;
}
renderer.setPixelRatio(pixelRatio());
renderer.setSize(window.innerWidth, window.innerHeight);
document.body.appendChild(renderer.domElement);
status.remove();

// the browser can take the gpu away (driver reset, too many tabs), say so instead of leaving a black screen
renderer.domElement.addEventListener('webglcontextlost', (e) => {
  e.preventDefault();
  showStatus('the graphics card dropped the drawing, reload the page to bring it back');
});
renderer.domElement.addEventListener('webglcontextrestored', () => status.remove());

const scene = new THREE.Scene();
const camera = new THREE.PerspectiveCamera(50, window.innerWidth / window.innerHeight, 0.01, 200);
camera.up.set(0, 0, 1);

// drag to orbit, scroll or pinch to zoom, works with mouse and touch
// zooming stops well before the far end of the camera's view, so the scene never gets cut off
const controls = new OrbitControls(camera, renderer.domElement);
controls.enableDamping = true;
controls.maxDistance = 60;

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
    // 1 tints heavy bodies warm, 0 for scenes where every body weighs the same and none stands out
    heavyTint: { value: 1 },
    // bodies this heavy or heavier are not drawn, so a black hole shows as its shadow and not a glowing dot
    hideMass: { value: Infinity },
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
    uniform float hideMass;
    varying float heavy;
    varying float fast;
    void main() {
      // a hidden body goes outside the view with no size, so nothing of it gets drawn
      if (m >= hideMass) {
        gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
        gl_PointSize = 0.0;
        return;
      }
      vec4 mv = modelViewMatrix * vec4(px, py, pz, 1.0);
      gl_Position = projectionMatrix * mv;
      // share of the total mass, cube rooted so a sun and a planet both stay visible
      heavy = pow(m / total, 1.0 / 3.0);
      // how fast this star goes next to a typical one, 0 at half speed or slower, 0.5 at typical, 1 at double or faster
      // the floor keeps log2 away from 0 for a body dropped in at rest
      float speed = max(length(vec3(vx, vy, vz)), 1e-6);
      fast = clamp(0.5 + 0.5 * log2(speed / typicalSpeed), 0.0, 1.0);
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
    uniform float heavyTint;
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
      gl_FragColor = vec4(mix(star, heavyColor, clamp(heavy * 2.0, 0.0, 1.0) * heavyTint) * a, 1.0);
      // turns the color into the screen's color space, so it looks the same with the effects on or off
      #include <colorspace_fragment>
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
// dim enough to stay under the glow threshold (brightness about 0.2 against 0.3), a fixed size so zooming does not change them
const skyMaterial = new THREE.PointsMaterial({ color: new THREE.Color(0.18, 0.2, 0.25), size: pixelRatio(), sizeAttenuation: false, depthWrite: false });
const sky = new THREE.Points(skyGeometry, skyMaterial);
scene.add(sky);

// bends the finished picture around a black hole, the way its gravity bends light (see physics-notes, drawing the black hole)
// everything inside the shadow is black, everything else is pulled from where a point mass lens says the light came from
const lens = new ShaderPass({
  uniforms: {
    tDiffuse: { value: null },
    // where the hole sits on screen, 0 to 1 across and up
    center: { value: new THREE.Vector2(0.5, 0.5) },
    // screen width over height, so circles stay round
    aspect: { value: 1 },
    // how many screen heights one radian of view covers near the middle
    perRadian: { value: 1 },
    // the shadow's angular radius, and the einstein radius squared, both in radians
    shadow: { value: 0 },
    einstein2: { value: 0 },
  },
  vertexShader: /* glsl */ `
    varying vec2 vUv;
    void main() {
      vUv = uv;
      gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0);
    }
  `,
  fragmentShader: /* glsl */ `
    uniform sampler2D tDiffuse;
    uniform vec2 center;
    uniform float aspect;
    uniform float perRadian;
    uniform float shadow;
    uniform float einstein2;
    varying vec2 vUv;
    void main() {
      // how far this pixel is from the hole, as an angle on the sky
      vec2 d = (vUv - center) * vec2(aspect, 1.0);
      float theta = length(d) / perRadian;
      // inside the shadow every ray falls in, so nothing comes back
      if (theta < shadow) {
        gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
      }
      // the photon ring, a thin band just outside the shadow made of light that looped around the hole
      // it is a squeezed picture of everything around the hole, so its brightness is the average light
      // sampled on a circle around the hole, an approximation since the lens formula below cannot make it
      if (theta < shadow * 1.12) {
        vec3 around = vec3(0.0);
        float reach = 2.0 * sqrt(einstein2) * perRadian;
        for (int i = 0; i < 12; i++) {
          float a = float(i) * 0.5236;
          around += texture2D(tDiffuse, center + vec2(cos(a), sin(a)) * reach / vec2(aspect, 1.0)).rgb;
        }
        // brightest right at the shadow's edge, fading out across the band
        float edge = 1.0 - (theta - shadow) / (shadow * 0.12);
        gl_FragColor = vec4(around / 12.0 * 16.0 * edge, 1.0);
        return;
      }
      // point mass lens: light seen at angle theta really came from beta, which goes negative (the far side) close in
      float beta = theta - einstein2 / theta;
      vec2 from = center + normalize(d) * beta * perRadian / vec2(aspect, 1.0);
      // light from off the edge of the picture is just dark space
      if (any(lessThan(from, vec2(0.0))) || any(greaterThan(from, vec2(1.0)))) {
        gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
      }
      // lensing moves light around without making it brighter per patch of sky, so it is a plain copy
      gl_FragColor = texture2D(tDiffuse, from);
    }
  `,
});
lens.enabled = false;

// the effects run after the scene is drawn: trails first, so the streaks glow too, then the glow,
// then the lens last, so the glow cannot spill back over the black shadow
const composer = new EffectComposer(renderer);
composer.addPass(new RenderPass(scene, camera));
const trails = new AfterimagePass(TRAIL_KEEP_PER_60TH);
trails.enabled = START_EFFECTS.trails;
composer.addPass(trails);
// strength, how far it spreads, and how bright a spot has to be before it glows
const glow = new UnrealBloomPass(new THREE.Vector2(window.innerWidth, window.innerHeight), 0.8, 0.2, 0.3);
glow.enabled = START_EFFECTS.glow;
composer.addPass(glow);
composer.addPass(lens);
composer.addPass(new OutputPass());

// set when the trails should start clean, so old streaks from before a switch or a new scene do not show up
let trailsFresh = true;

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

  // when every body weighs the same (the figure-eight) none of them is the heavy one, so skip the warm tint
  let lightest = Infinity;
  let heaviest = 0;
  for (const m of masses) {
    lightest = Math.min(lightest, m);
    heaviest = Math.max(heaviest, m);
  }
  material.uniforms.heavyTint.value = heaviest > lightest * 1.001 ? 1 : 0;
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

// whether the panel has lensing on, it only does anything on a scene with a black hole
let lensing = START_EFFECTS.lensing;
const hole = new THREE.Vector3();
const toHole = new THREE.Vector3();
const facing = new THREE.Vector3();

// finds the black hole (body 0) on screen each frame and works out how big its shadow and lensing look from here
function updateLens() {
  const c = preset.lightSpeed;
  const masses = geometry.getAttribute('m').array as Float32Array;
  // its mass in length units, GM / c^2 with G = 1, the M in every black hole formula
  const m = c ? masses[0] / (c * c) : 0;
  hole.set(
    (geometry.getAttribute('px').array as Float32Array)[0],
    (geometry.getAttribute('py').array as Float32Array)[0],
    (geometry.getAttribute('pz').array as Float32Array)[0],
  );
  toHole.subVectors(hole, camera.position);
  const dist = toHole.length();
  camera.getWorldDirection(facing);

  // off when the switch is off, the scene has no hole, the hole is behind the camera, or the camera is nearly inside it
  lens.enabled = lensing && c !== undefined && toHole.dot(facing) > 0 && dist > 4 * m;
  material.uniforms.hideMass.value = lens.enabled ? masses[0] : Infinity;
  if (!lens.enabled) return;

  camera.updateMatrixWorld();
  hole.project(camera);
  const u = lens.uniforms;
  u.center.value.set((hole.x + 1) / 2, (hole.y + 1) / 2);
  u.aspect.value = camera.aspect;
  u.perRadian.value = 0.5 / Math.tan(THREE.MathUtils.degToRad(camera.fov) / 2);
  // the shadow's size for a camera at rest at this distance, sin(a) = (3 sqrt(3) M / r) sqrt(1 - 2M / r)
  // the same formula event-horizon tests its ray tracer against
  u.shadow.value = Math.asin(Math.min(1, ((3 * Math.sqrt(3) * m) / dist) * Math.sqrt(1 - (2 * m) / dist)));
  // einstein radius squared, 4M D_ls / (D_l D_s), taking whatever is behind to sit as far behind the hole as the camera is in front
  u.einstein2.value = (2 * m) / dist;
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
  material.uniforms.size.value = preset.dot * pixelRatio();
  // tall narrow screens see less side to side, so the camera backs off until the scene fits across
  const back = preset.camera * Math.max(1, 1 / camera.aspect);
  camera.position.set(0, -0.8 * back, 0.6 * back);
  controls.target.set(0, 0, 0);
  simTime = 0;
  trailsFresh = true;
  resetBaseline();
}

// what the panel is allowed to change
const actions: Actions = {
  load,
  setPaused: (p) => (paused = p),
  setSpeed: (s) => (speed = s),
  setTheta: (t) => (theta = t),
  setGlow: (on) => (glow.enabled = on),
  setLensing: (on) => (lensing = on),
  setTrails: (on) => {
    trails.enabled = on;
    trailsFresh = true;
  },
  setSpeedColors: (on) => {
    material.uniforms.speedColors.value = on ? 1 : 0;
    // the velocities were not being sent to the gpu while this was off, so send them now
    for (const name of ['vx', 'vy', 'vz']) geometry.getAttribute(name).needsUpdate = true;
  },
};
load(START_PRESET, defaultN(START_PRESET));
createRoot(document.getElementById('panel')!).render(createElement(Panel, { actions }));

// keeps the picture the right shape and sharpness when the window changes size or moves to another screen
window.addEventListener('resize', () => {
  camera.aspect = window.innerWidth / window.innerHeight;
  camera.updateProjectionMatrix();
  renderer.setPixelRatio(pixelRatio());
  renderer.setSize(window.innerWidth, window.innerHeight);
  composer.setPixelRatio(pixelRatio());
  composer.setSize(window.innerWidth, window.innerHeight);
  material.uniforms.size.value = preset.dot * pixelRatio();
  skyMaterial.size = pixelRatio();
});

// a click or tap that did not turn into a drag drops a body where the pointer meets the flat z = 0 plane
const raycaster = new THREE.Raycaster();
const ground = new THREE.Plane(new THREE.Vector3(0, 0, 1), 0);
let downX = 0;
let downY = 0;
// fingers on the screen right now, and whether a second one joined, which makes it a pinch and not a tap
const pointersDown = new Set<number>();
let pinched = false;
renderer.domElement.addEventListener('pointerdown', (e) => {
  pointersDown.add(e.pointerId);
  if (pointersDown.size > 1) pinched = true;
  if (e.button !== 0 || !e.isPrimary) return;
  downX = e.clientX;
  downY = e.clientY;
});
// a finger that slides off the page or gets taken by the browser never spawns anything
renderer.domElement.addEventListener('pointercancel', (e) => {
  pointersDown.delete(e.pointerId);
  if (pointersDown.size === 0) pinched = false;
});
renderer.domElement.addEventListener('pointerup', (e) => {
  pointersDown.delete(e.pointerId);
  const wasPinch = pinched;
  if (pointersDown.size === 0) pinched = false;

  // only a plain left click or a one finger tap, the other buttons pan the camera
  if (wasPinch || e.button !== 0 || !e.isPrimary) return;
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
  const elapsed = Math.max(0, now - last) / 1000;
  backlog = paused ? 0 : backlog + elapsed;
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

    // tell three.js the positions changed so it sends them to the gpu again, velocities only when the colors use them
    for (const name of ['px', 'py', 'pz']) geometry.getAttribute(name).needsUpdate = true;
    if (material.uniforms.speedColors.value) for (const name of ['vx', 'vy', 'vz']) geometry.getAttribute(name).needsUpdate = true;
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

  // trails fade by real time, so they are the same length on a 60 Hz and a 180 Hz screen
  // a fresh start keeps nothing from before, so no old streaks flash up
  trails.damp = trailsFresh ? 0 : Math.pow(TRAIL_KEEP_PER_60TH, Math.min(elapsed, 0.25) * 60);
  if (trails.enabled) trailsFresh = false;

  controls.update();
  // the background stars ride along with the camera so they always look infinitely far away
  sky.position.copy(camera.position);
  updateLens();
  // straight to the screen when every effect is off, through the effects otherwise
  if (glow.enabled || trails.enabled || lens.enabled) composer.render();
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
