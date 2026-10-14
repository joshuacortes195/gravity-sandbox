#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "bodies.h"
#include "diagnostics.h"
#include "integrator.h"
#include "octree.h"
#include "scenes.h"

// builds one of the browser's presets and runs it the way the page does, through the tree,
// printing the starting energy and how far it drifts, so each preset's timestep and softening can be picked
// usage: preset_check name n dt eps theta time checkpoints
// names: solar, eight, collision, cluster, disk. n is ignored by solar and eight
int main(int argc, char** argv) {
  if (argc != 8) {
    std::fprintf(stderr, "usage: preset_check solar|eight|collision|cluster|disk n dt eps theta time checkpoints\n");
    return 1;
  }
  const char* name = argv[1];
  const int n = std::atoi(argv[2]);
  const float dt = float(std::atof(argv[3])), eps = float(std::atof(argv[4])), theta = float(std::atof(argv[5]));
  const double time = std::atof(argv[6]);
  const int checkpoints = std::atoi(argv[7]);

  // the same scenes and settings the page loads
  Bodies b;
  if (!std::strcmp(name, "solar")) b = solar_system();
  else if (!std::strcmp(name, "eight")) b = figure_eight();
  else if (!std::strcmp(name, "collision")) b = galaxy_collision(n, 2.5f, 0.8f, eps, 1);
  else if (!std::strcmp(name, "cluster")) b = black_hole_cluster(n, 1.5f, 0.15f, 1);
  else if (!std::strcmp(name, "disk")) b = disk_galaxy(n, 1.0f, 0.005f, 1.0f, eps, 1);
  else {
    std::fprintf(stderr, "unknown preset %s\n", name);
    return 1;
  }

  // starting energy split into motion and pull, to see how far from zero the total sits
  Octree tree;
  compute_accel_tree(b, tree, eps, theta);
  const double k0 = kinetic_energy(b), w0 = potential_energy(b, eps), e0 = k0 + w0;
  std::printf("preset %s, bodies %zu, dt %g, eps %g, theta %g\n", name, b.size(), dt, eps, theta);
  std::printf("starting energy %.6g (kinetic %.6g, potential %.6g)\n", e0, k0, w0);
  std::printf("%10s %14s %12s\n", "time", "energy drift", "ms per step");

  // runs in equal chunks and prints the drift relative to the starting energy after each one
  const long long steps = std::llround(time / dt);
  long long done = 0;
  for (int c = 1; c <= checkpoints; ++c) {
    const long long target = steps * c / checkpoints;
    const auto t0 = std::chrono::steady_clock::now();
    const long long chunk = target - done;
    for (; done < target; ++done) step(b, tree, dt, eps, theta);
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    const double drift = (total_energy(b, eps) - e0) / std::fabs(e0);
    std::printf("%10.3f %14.3e %12.3f\n", done * dt, drift, chunk ? ms / chunk : 0.0);
  }
  return 0;
}
