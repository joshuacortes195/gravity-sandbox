// runs a disk galaxy and prints how much of it is still a disk, four times over the run
// usage: disk_check n disk_mass eps dt theta steps   (theta below 0 means brute force)
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "diagnostics.h"
#include "gravity.h"
#include "integrator.h"
#include "octree.h"
#include "scenes.h"

int main(int argc, char** argv) {
  if (argc != 7) {
    std::printf("usage: disk_check n disk_mass eps dt theta steps\n");
    return 1;
  }
  const int n = std::atoi(argv[1]);
  const float disk_mass = std::atof(argv[2]);
  const float eps = std::atof(argv[3]);
  const float dt = std::atof(argv[4]);
  const float theta = std::atof(argv[5]);
  const int steps = std::atoi(argv[6]);
  const bool brute = theta < 0;

  // same disk the browser opens on, center of mass 1 and radius 1
  Bodies b = disk_galaxy(n, 1.0f, disk_mass, 1.0f, eps, 1);

  // where every star started, to see how far it wandered
  std::vector<float> r0(b.size());
  for (std::size_t i = 1; i < b.size(); ++i) r0[i] = std::hypot(b.x[i], b.y[i]);

  const double e0 = total_energy(b, eps);
  Octree tree;
  if (brute) compute_accel(b, eps); else compute_accel_tree(b, tree, eps, theta);

  std::printf("n %d, disk mass %g, eps %g, dt %g, %s\n", n, disk_mass, eps, dt, brute ? "brute force" : "tree");
  for (int s = 1; s <= steps; ++s) {
    if (brute) step(b, dt, eps); else step(b, tree, dt, eps, theta);

    // every quarter of the run: share of stars whose radius moved over 10%, the farthest star, and energy drift
    if (s % (steps / 4) == 0) {
      int moved = 0;
      float farthest = 0;
      for (std::size_t i = 1; i < b.size(); ++i) {
        const float r = std::hypot(b.x[i], b.y[i]);
        if (std::fabs(r - r0[i]) / r0[i] > 0.1f) ++moved;
        farthest = std::fmax(farthest, r);
      }
      std::printf("t %.2f  moved over 10%%: %.1f%%  farthest %.2f  energy drift %.2e\n", s * dt, 100.0 * moved / n,
                  farthest, (total_energy(b, eps) - e0) / std::fabs(e0));
    }
  }
}
