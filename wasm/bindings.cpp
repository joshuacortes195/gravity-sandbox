#include <emscripten/emscripten.h>

#include <algorithm>

#include "bodies.h"
#include "diagnostics.h"
#include "integrator.h"
#include "octree.h"
#include "scenes.h"
#include "threads.h"

// the one sim the page drives, plus its tree so the tree memory is reused every step
static Bodies bodies;
static Octree tree;

extern "C" {

// swaps in one of the presets and works out its starting pull, so the first step is right
// 0 solar system, 1 figure-eight, 2 galaxy collision (n stars per disk), 3 black hole and cluster, 4 one disk galaxy
// n changes here, so the page has to grab new views of x, y and z afterwards
EMSCRIPTEN_KEEPALIVE void load_scene(int which, int n, unsigned seed, float eps, float theta) {
  switch (which) {
    case 0: bodies = solar_system(); break;
    case 1: bodies = figure_eight(); break;
    case 2: bodies = galaxy_collision(n, 2.5f, 0.8f, eps, seed); break;
    case 3: bodies = black_hole_cluster(n, 1.5f, 0.15f, seed); break;
    // the disk is kept light, a heavier cold disk clumps up within one turn
    default: bodies = disk_galaxy(n, 1.0f, 0.005f, 1.0f, eps, seed); break;
  }
  compute_accel_tree(bodies, tree, eps, theta);
}

// drops a new body into the running sim and redoes the pull so the next step includes it
// the arrays can move when they grow, so the page has to grab new views afterwards
EMSCRIPTEN_KEEPALIVE void spawn(float x, float y, float z, float vx, float vy, float vz, float m, float eps, float theta) {
  bodies.add(x, y, z, vx, vy, vz, m);
  compute_accel_tree(bodies, tree, eps, theta);
}

// runs the sim forward a few fixed steps in one call, so js only crosses into wasm once a frame
EMSCRIPTEN_KEEPALIVE void advance(int steps, float dt, float eps, float theta) {
  for (int s = 0; s < steps; ++s) step(bodies, tree, dt, eps, theta);
}

// same steps with brute force instead of the tree, only the benchmark page uses it
EMSCRIPTEN_KEEPALIVE void advance_brute(int steps, float dt, float eps) {
  for (int s = 0; s < steps; ++s) step(bodies, dt, eps);
}

// how many threads share the force pass, only the threaded build can go above 1
EMSCRIPTEN_KEEPALIVE void threads(int k) { set_threads(k); }

// how many bodies there are right now
EMSCRIPTEN_KEEPALIVE int body_count() { return int(bodies.size()); }

// where each position array sits in wasm memory, so js can read them in place
EMSCRIPTEN_KEEPALIVE float* pos_x() { return bodies.x.data(); }
EMSCRIPTEN_KEEPALIVE float* pos_y() { return bodies.y.data(); }
EMSCRIPTEN_KEEPALIVE float* pos_z() { return bodies.z.data(); }

// masses too, so heavy bodies can be drawn bigger
EMSCRIPTEN_KEEPALIVE float* mass() { return bodies.m.data(); }

// the total energy is an o(n^2) sum, too slow for one frame with thousands of bodies,
// so it runs a few rows per frame on a frozen copy while the real sim keeps moving
static Bodies frozen;
static std::size_t energy_row = 0;
static double energy_sum = 0.0;

// freezes the bodies as they are right now and starts the sum with their energy of motion
EMSCRIPTEN_KEEPALIVE void energy_begin() {
  frozen = bodies;
  energy_row = 0;
  energy_sum = kinetic_energy(frozen);
}

// adds the next rows of the pull energy, returns 1 once every row is in
EMSCRIPTEN_KEEPALIVE int energy_work(int rows, float eps) {
  const std::size_t end = std::min(frozen.size(), energy_row + std::size_t(rows));
  energy_sum += potential_rows(frozen, eps, energy_row, end);
  energy_row = end;
  return energy_row == frozen.size();
}

// the finished total, only meaningful once energy_work has returned 1
EMSCRIPTEN_KEEPALIVE double energy_value() { return energy_sum; }
}
