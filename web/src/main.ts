import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';
import createSim from './wasm/sim.js';

// sim settings: body count, softening, tree accuracy and the one fixed timestep
const N = 2000;
const EPS = 0.01;
const THETA = 0.7;
const DT = 0.002;

// how many physics steps run per real second, and the most one frame may run so a slow frame cannot snowball
const STEPS_PER_SECOND = 120;
const MAX_STEPS_PER_FRAME = 8;

const sim = await createSim();

// renderer, scene and a camera looking down at the disk from an angle
const renderer = new THREE.WebGLRenderer({ antialias: false });
renderer.setPixelRatio(window.devicePixelRatio);
renderer.setSize(window.innerWidth, window.innerHeight);
document.body.appendChild(renderer.domElement);

const scene = new THREE.Scene();
const camera = new THREE.PerspectiveCamera(50, window.innerWidth / window.innerHeight, 0.01, 100);
camera.up.set(0, 0, 1);
camera.position.set(0, -2.2, 1.6);

// drag to orbit, scroll or pinch to zoom, works with mouse and touch
const controls = new OrbitControls(camera, renderer.domElement);
controls.enableDamping = true;

// one point per body, the shader builds each position from the three separate x, y, z arrays
const geometry = new THREE.BufferGeometry();
const material = new THREE.ShaderMaterial({
  uniforms: {
    size: { value: 3 * window.devicePixelRatio },
    color: { value: new THREE.Color(0.55, 0.7, 1.0) },
  },
  vertexShader: /* glsl */ `
    attribute float px;
    attribute float py;
    attribute float pz;
    uniform float size;
    void main() {
      vec4 mv = modelViewMatrix * vec4(px, py, pz, 1.0);
      gl_Position = projectionMatrix * mv;
      // farther bodies draw smaller
      gl_PointSize = size / -mv.z;
    }
  `,
  fragmentShader: /* glsl */ `
    uniform vec3 color;
    void main() {
      // soft round dot instead of a square
      float a = 1.0 - smoothstep(0.3, 0.5, length(gl_PointCoord - 0.5));
      if (a <= 0.0) discard;
      gl_FragColor = vec4(color * a, 1.0);
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
  geometry.setDrawRange(0, n);
}

// starts the disk galaxy and hooks it up to the renderer
sim._load_disk(N, 1, EPS, THETA);
attachViews();

// keeps the picture the right shape when the window changes size
window.addEventListener('resize', () => {
  camera.aspect = window.innerWidth / window.innerHeight;
  camera.updateProjectionMatrix();
  renderer.setSize(window.innerWidth, window.innerHeight);
});

// numbers for the corner readout, reset every half second
const stats = document.getElementById('stats')!;
let frames = 0;
let stepsDone = 0;
let physicsMs = 0;
let windowStart = performance.now();

// real time that has passed but has not been simulated yet
let backlog = 0;
let last = performance.now();

// runs once per screen refresh: catch the physics up in fixed steps, then draw
function frame(now: number) {
  backlog += (now - last) / 1000;
  last = now;

  // as many whole steps as the backlog covers, but never more than the cap
  let steps = Math.floor(backlog * STEPS_PER_SECOND);
  if (steps > MAX_STEPS_PER_FRAME) {
    // too far behind, drop the extra time so the sim slows down instead of locking up
    steps = MAX_STEPS_PER_FRAME;
    backlog = 0;
  } else {
    backlog -= steps / STEPS_PER_SECOND;
  }

  // one call into wasm for all of this frame's steps, timed for the readout
  if (steps > 0) {
    const t0 = performance.now();
    sim._advance(steps, DT, EPS, THETA);
    physicsMs += performance.now() - t0;
    stepsDone += steps;

    // tell three.js the positions changed so it sends them to the gpu again
    for (const name of ['px', 'py', 'pz']) geometry.getAttribute(name).needsUpdate = true;
  }

  controls.update();
  renderer.render(scene, camera);

  // updates the fps readout twice a second
  frames++;
  if (now - windowStart >= 500) {
    const seconds = (now - windowStart) / 1000;
    stats.textContent =
      `bodies ${sim._body_count()}\n` +
      `fps ${(frames / seconds).toFixed(1)}\n` +
      `physics ${(stepsDone ? physicsMs / stepsDone : 0).toFixed(2)} ms/step\n` +
      `steps/s ${(stepsDone / seconds).toFixed(0)}`;
    frames = 0;
    stepsDone = 0;
    physicsMs = 0;
    windowStart = now;
  }

  requestAnimationFrame(frame);
}
requestAnimationFrame(frame);
