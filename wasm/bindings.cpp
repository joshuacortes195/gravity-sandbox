#include <emscripten/emscripten.h>

#include "bodies.h"
#include "integrator.h"
#include "octree.h"
#include "scenes.h"

// the one sim the page drives, plus its tree so the tree memory is reused every step
static Bodies bodies;
static Octree tree;

extern "C" {

// swaps in a fresh disk galaxy and works out its starting pull, so the first step is right
// the disk is kept light, a heavier cold disk clumps up within one turn
// n changes here, so the page has to grab new views of x, y and z afterwards
EMSCRIPTEN_KEEPALIVE void load_disk(int n, unsigned seed, float eps, float theta) {
  bodies = disk_galaxy(n, 1.0f, 0.005f, 1.0f, eps, seed);
  compute_accel_tree(bodies, tree, eps, theta);
}

// runs the sim forward a few fixed steps in one call, so js only crosses into wasm once a frame
EMSCRIPTEN_KEEPALIVE void advance(int steps, float dt, float eps, float theta) {
  for (int s = 0; s < steps; ++s) step(bodies, tree, dt, eps, theta);
}

// how many bodies there are right now
EMSCRIPTEN_KEEPALIVE int body_count() { return int(bodies.size()); }

// where each position array sits in wasm memory, so js can read them in place
EMSCRIPTEN_KEEPALIVE float* pos_x() { return bodies.x.data(); }
EMSCRIPTEN_KEEPALIVE float* pos_y() { return bodies.y.data(); }
EMSCRIPTEN_KEEPALIVE float* pos_z() { return bodies.z.data(); }
}
