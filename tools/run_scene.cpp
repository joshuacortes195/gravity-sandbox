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
  const double speed_scale = argc > 1 ? std::atof(argv[1]) : 1.0;
  const int per_orbit = argc > 2 ? std::atoi(argv[2]) : 1000;
  const int orbits = argc > 3 ? std::atoi(argv[3]) : 100;

  // bad input gets a usage line, a speed of sqrt(2) or more would fly apart and never orbit
  if (speed_scale <= 0.0 || speed_scale >= 1.4 || per_orbit <= 0 || orbits <= 0) {
    std::fprintf(stderr, "usage: run_scene [speed_scale 0 to 1.4] [steps_per_orbit] [orbits]\n");
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
  const long steps = long(per_orbit) * orbits;

  // starting totals to compare against
  compute_accel(b, eps);
  const double e0 = total_energy(b, eps);
  const auto p0 = momentum(b);
  const auto l0 = angular_momentum(b);

  // the run itself, keeping track of the worst energy error along the way
  double worst = 0.0;
  for (long s = 0; s < steps; ++s) {
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
  std::printf("timestep           %.6g (%d steps per orbit)\n", dt, per_orbit);
  std::printf("steps              %ld (%d orbits)\n", steps, orbits);
  std::printf("starting energy    %.9f\n", e0);
  std::printf("final energy drift %.3e\n", (e1 - e0) / e0);
  std::printf("worst energy drift %.3e\n", worst);
  std::printf("momentum change    %.3e\n", std::hypot(p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2]));
  std::printf("spin change        %.3e (of %.6f)\n", l1[2] - l0[2], l0[2]);
  std::printf("distance from start %.3e (light body, after whole orbits)\n", miss);
  return 0;
}
