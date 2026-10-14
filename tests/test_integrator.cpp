#include <algorithm>
#include <cmath>

#include "doctest.h"
#include "bodies.h"
#include "diagnostics.h"
#include "gravity.h"
#include "integrator.h"
#include "scenes.h"

const double pi = 3.14159265358979323846;

// runs the sim forward a number of steps, forces have to be current going in
static void run(Bodies& b, int steps, float dt, float eps) {
  for (int s = 0; s < steps; ++s) step(b, dt, eps);
}

// total mass 1 and separation 1 give a period of exactly 2 pi
TEST_CASE("a circular two body orbit returns to its start after one period") {
  Bodies b = two_body_orbit(0.75f, 0.25f, 1.0f, 1.0f);
  const Bodies start = b;

  const int steps = 2000;
  const float dt = float(2.0 * pi / steps);
  compute_accel(b, 0.0f);
  run(b, steps, dt, 0.0f);

  for (int i = 0; i < 2; ++i) {
    CHECK(b.x[i] == doctest::Approx(start.x[i]).epsilon(1e-3));
    CHECK(b.y[i] == doctest::Approx(start.y[i]).scale(1.0).epsilon(1e-3));
    CHECK(b.z[i] == 0.0f);
  }
}

// a quarter of the way round the bodies have to be somewhere else, or the
// test above would pass for a sim that never moves
TEST_CASE("a quarter period moves the bodies a quarter turn") {
  Bodies b = two_body_orbit(0.75f, 0.25f, 1.0f, 1.0f);

  const int steps = 2000;
  const float dt = float(2.0 * pi / steps);
  compute_accel(b, 0.0f);
  run(b, steps / 4, dt, 0.0f);

  // the light body started at x = 0.75 moving toward +y
  CHECK(b.x[1] == doctest::Approx(0.0).scale(1.0).epsilon(1e-3));
  CHECK(b.y[1] == doctest::Approx(0.75).epsilon(1e-3));
}

// biggest energy error seen during a run, as a fraction of the starting energy
static double worst_energy_drift(Bodies& b, int steps, float dt, float eps) {
  compute_accel(b, eps);
  const double e0 = total_energy(b, eps);
  double worst = 0.0;
  for (int s = 0; s < steps; ++s) {
    step(b, dt, eps);
    worst = std::max(worst, std::abs((total_energy(b, eps) - e0) / e0));
  }
  return worst;
}

// a stretched orbit: speed 0.7 of circular gives a period of 2 pi * a^1.5 with a = 1 / (2 - 0.49)
const double stretched_period = 2.0 * pi * std::pow(1.0 / (2.0 - 0.49), 1.5);

// leapfrog's energy error wobbles but does not pile up, so a 10 times longer run is no worse
TEST_CASE("energy drift stays bounded over a long run") {
  const int per_orbit = 500;
  const float dt = float(stretched_period / per_orbit);

  Bodies shorter = two_body_orbit(0.75f, 0.25f, 1.0f, 0.7f);
  Bodies longer = shorter;
  const double drift10 = worst_energy_drift(shorter, 10 * per_orbit, dt, 0.0f);
  const double drift100 = worst_energy_drift(longer, 100 * per_orbit, dt, 0.0f);
  MESSAGE("worst drift over 10 orbits " << drift10 << ", over 100 orbits " << drift100);

  CHECK(drift10 < 1e-3);
  CHECK(drift100 < 1.5 * drift10);
}

// halving the timestep should cut the energy error by 4, on a coarse stretched orbit
// so the error is well above 32 bit rounding
TEST_CASE("leapfrog is second order") {
  const int per_orbit = 200;
  const float dt = float(stretched_period / per_orbit);

  Bodies coarse = two_body_orbit(0.75f, 0.25f, 1.0f, 0.7f);
  Bodies fine = coarse;
  const double drift_coarse = worst_energy_drift(coarse, per_orbit, dt, 0.0f);
  const double drift_fine = worst_energy_drift(fine, 2 * per_orbit, 0.5f * dt, 0.0f);
  MESSAGE("coarse " << drift_coarse << ", fine " << drift_fine << ", ratio " << drift_coarse / drift_fine);

  CHECK(drift_coarse / drift_fine > 3.5);
  CHECK(drift_coarse / drift_fine < 4.5);
}

// four bodies with a net motion and a net spin, softened so no close pass dominates
static Bodies small_cluster() {
  Bodies b;
  b.add(0.0f, 0.0f, 0.0f, 0.1f, 0.0f, 0.05f, 5.0f);
  b.add(1.0f, 0.2f, -0.3f, 0.0f, 2.0f, 0.3f, 0.5f);
  b.add(-0.7f, 1.1f, 0.4f, -1.5f, -0.5f, 0.0f, 1.5f);
  b.add(0.3f, -0.9f, 0.8f, 1.8f, 0.6f, -0.4f, 0.1f);
  return b;
}

TEST_CASE("momentum and angular momentum are conserved") {
  const float eps = 0.05f;
  Bodies b = small_cluster();
  const auto p0 = momentum(b);
  const auto l0 = angular_momentum(b);

  // the scene has to actually carry some of each, or the check means nothing
  REQUIRE(std::abs(p0[0]) > 0.1);
  REQUIRE(std::abs(l0[2]) > 0.1);

  compute_accel(b, eps);
  run(b, 5000, 1e-3f, eps);
  const auto p1 = momentum(b);
  const auto l1 = angular_momentum(b);

  // only 32 bit rounding is allowed to leak in
  for (int k = 0; k < 3; ++k) {
    CHECK(std::abs(p1[k] - p0[k]) < 1e-4);
    CHECK(std::abs(l1[k] - l0[k]) < 1e-4);
  }
}

// leapfrog retraces its own path when the velocities are flipped
TEST_CASE("running forward then backward returns to the start") {
  const float eps = 0.05f;
  Bodies b = small_cluster();
  const Bodies start = b;

  compute_accel(b, eps);
  run(b, 1000, 1e-3f, eps);

  // it has to have gone somewhere first
  REQUIRE(std::abs(b.x[1] - start.x[1]) > 0.1f);

  for (std::size_t i = 0; i < b.size(); ++i) {
    b.vx[i] = -b.vx[i];
    b.vy[i] = -b.vy[i];
    b.vz[i] = -b.vz[i];
  }
  run(b, 1000, 1e-3f, eps);

  for (std::size_t i = 0; i < b.size(); ++i) {
    CHECK(std::abs(b.x[i] - start.x[i]) < 1e-4f);
    CHECK(std::abs(b.y[i] - start.y[i]) < 1e-4f);
    CHECK(std::abs(b.z[i] - start.z[i]) < 1e-4f);
  }
}
