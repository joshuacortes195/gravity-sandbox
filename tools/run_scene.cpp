#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "bodies.h"
#include "diagnostics.h"
#include "gravity.h"
#include "integrator.h"
#include "scenes.h"

// runs the two body scene and prints how well energy and momentum held up
// usage: run_scene [speed_scale] [steps_per_orbit] [orbits]
int main(int argc, char** argv) {
  // defaults are a circular orbit, 1 is circular and lower is more stretched
  double speed_scale = 1.0;
  long long per_orbit = 1000, orbits = 100;

  // reads the arguments, anything left over after the number (like 1e3 or 10x) counts as bad
  bool ok = true;
  char* end = nullptr;
  if (argc > 1) { speed_scale = std::strtod(argv[1], &end); ok = ok && *end == '\0'; }
  if (argc > 2) { per_orbit = std::strtoll(argv[2], &end, 10); ok = ok && *end == '\0'; }
  if (argc > 3) { orbits = std::strtoll(argv[3], &end, 10); ok = ok && *end == '\0'; }

  // a speed of sqrt(2) or more would fly apart and never orbit, and the step cap keeps a typo from running for hours
  const long long max_steps = 100000000;
  ok = ok && argc <= 4 && speed_scale > 0.0 && speed_scale < 1.4;
  ok = ok && per_orbit > 0 && orbits > 0 && per_orbit <= max_steps && orbits <= max_steps / per_orbit;
  if (!ok) {
    std::fprintf(stderr, "usage: run_scene [speed_scale 0 to 1.4] [steps_per_orbit] [orbits]\n");
    std::fprintf(stderr, "whole numbers for the last two, at most %lld steps in total\n", max_steps);
    return 1;
  }

  // the scene: masses 0.75 and 0.25, one unit apart, no softening
  const float m1 = 0.75f, m2 = 0.25f, separation = 1.0f, eps = 0.0f;
  Bodies b = two_body_orbit(m1, m2, separation, float(speed_scale));
  const Bodies start = b;

  // period from the size of the orbit, then a timestep that divides it evenly
  const double pi = 3.14159265358979323846;
  const double a = separation / (2.0 - speed_scale * speed_scale);
  const double period = 2.0 * pi * std::sqrt(a * a * a / (m1 + m2));
  const float dt = float(period / per_orbit);
  const long long steps = per_orbit * orbits;

  // starting totals to compare against
  compute_accel(b, eps);
  const double e0 = total_energy(b, eps);
  const auto p0 = momentum(b);
  const auto l0 = angular_momentum(b);

  // the run itself, keeping track of the worst energy error along the way
  double worst = 0.0;
  for (long long s = 0; s < steps; ++s) {
    step(b, dt, eps);
    worst = std::max(worst, std::abs((total_energy(b, eps) - e0) / e0));
  }

  // where things ended up
  const double e1 = total_energy(b, eps);
  const auto p1 = momentum(b);
  const auto l1 = angular_momentum(b);
  const double miss = std::hypot(double(b.x[1]) - start.x[1], double(b.y[1]) - start.y[1]);

  std::printf("scene              two body, m = %g and %g, separation %g, speed scale %g\n", m1, m2, separation, speed_scale);
  std::printf("softening          %g\n", eps);
  std::printf("period             %.6f\n", period);
  std::printf("timestep           %.6g (%lld steps per orbit)\n", dt, per_orbit);
  std::printf("steps              %lld (%lld orbits)\n", steps, orbits);
  std::printf("starting energy    %.9f\n", e0);
  std::printf("final energy drift %.3e\n", (e1 - e0) / e0);
  std::printf("worst energy drift %.3e\n", worst);
  std::printf("momentum change    %.3e\n", std::hypot(p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2]));
  std::printf("spin change        %.3e (of %.6f)\n", l1[2] - l0[2], l0[2]);
  std::printf("distance from start %.3e (light body, after whole orbits)\n", miss);
  return 0;
}
