#include <algorithm>
#include <cmath>
#include <vector>

#include "doctest.h"
#include "bodies.h"
#include "diagnostics.h"
#include "gravity.h"
#include "integrator.h"
#include "scenes.h"

TEST_CASE("a disk galaxy is a heavy center plus a flat ring of light stars") {
  const int n = 500;
  const Bodies b = disk_galaxy(n, 1.0f, 0.1f, 2.0f, 0.01f, 7);

  // the center comes first, sitting at the origin
  REQUIRE(b.size() == n + 1);
  CHECK(b.m[0] == 1.0f);
  CHECK(b.x[0] == 0.0f);
  CHECK(b.y[0] == 0.0f);
  CHECK(b.z[0] == 0.0f);

  // every star gets an equal share of the disk mass, lies flat and stays between the inner edge and the rim
  for (int i = 1; i <= n; ++i) {
    CHECK(b.m[i] == doctest::Approx(0.1f / n));
    CHECK(b.z[i] == 0.0f);
    CHECK(b.vz[i] == 0.0f);
    const float r = std::sqrt(b.x[i] * b.x[i] + b.y[i] * b.y[i]);
    CHECK(r >= 0.1f * 2.0f);
    CHECK(r <= 2.0f);
  }
}

TEST_CASE("disk stars around a lone center move at the softened circular speed, sideways, all the same way") {
  const float eps = 0.05f;
  const Bodies b = disk_galaxy(200, 4.0f, 0.0f, 1.0f, eps, 3);

  for (std::size_t i = 1; i < b.size(); ++i) {
    const float r2 = b.x[i] * b.x[i] + b.y[i] * b.y[i];
    const float speed = std::sqrt(b.vx[i] * b.vx[i] + b.vy[i] * b.vy[i]);

    // the speed where the softened pull of the center, M r / (r^2 + eps^2)^1.5, exactly bends the path
    const float expected = std::sqrt(4.0f * r2 / std::pow(r2 + eps * eps, 1.5f));
    CHECK(speed == doctest::Approx(expected).epsilon(1e-5));

    // moving at right angles to the line from the center, and counterclockwise seen from above
    CHECK(std::fabs(b.x[i] * b.vx[i] + b.y[i] * b.vy[i]) <= 1e-5f * std::sqrt(r2) * speed);
    CHECK(b.x[i] * b.vy[i] - b.y[i] * b.vx[i] > 0.0f);
  }
}

TEST_CASE("disk stars also feel the part of the disk inside their orbit") {
  const float eps = 0.05f, center = 1.0f, disk = 0.5f, radius = 1.0f, inner = 0.1f;
  const Bodies b = disk_galaxy(200, center, disk, radius, eps, 9);

  for (std::size_t i = 1; i < b.size(); ++i) {
    const float r2 = b.x[i] * b.x[i] + b.y[i] * b.y[i];
    const float speed = std::sqrt(b.vx[i] * b.vx[i] + b.vy[i] * b.vy[i]);

    // stars are spread evenly over the ring's area, so the share of the disk inside radius r is the share of the area
    const float share = (r2 - inner * inner) / (radius * radius - inner * inner);
    const float expected = std::sqrt((center + share * disk) * r2 / std::pow(r2 + eps * eps, 1.5f));
    CHECK(speed == doctest::Approx(expected).epsilon(1e-4));
  }
}

TEST_CASE("a disk galaxy has no net momentum, so it does not drift off") {
  const Bodies b = disk_galaxy(300, 1.0f, 0.5f, 1.0f, 0.01f, 11);

  // a heavy disk with random star spots leaves some leftover momentum unless the center cancels it
  const auto p = momentum(b);
  CHECK(std::fabs(p[0]) < 1e-6);
  CHECK(std::fabs(p[1]) < 1e-6);
  CHECK(std::fabs(p[2]) < 1e-6);
}

TEST_CASE("massless disk stars stay on their circles for a whole inner orbit") {
  const float eps = 0.05f;
  Bodies b = disk_galaxy(100, 1.0f, 0.0f, 1.0f, eps, 5);

  // starting radius of every star
  std::vector<float> r0(b.size());
  for (std::size_t i = 1; i < b.size(); ++i) r0[i] = std::hypot(b.x[i], b.y[i]);

  // one orbit at the inner edge, 2 pi sqrt(r^3 / M), in 2000 steps
  const float period = 6.2831853f * std::sqrt(0.1f * 0.1f * 0.1f / 1.0f);
  const int steps = 2000;
  compute_accel(b, eps);
  for (int s = 0; s < steps; ++s) step(b, period / steps, eps);

  // a circular orbit keeps its radius, a wrong starting speed makes it swing in and out
  float worst = 0;
  for (std::size_t i = 1; i < b.size(); ++i) {
    worst = std::max(worst, std::fabs(std::hypot(b.x[i], b.y[i]) - r0[i]) / r0[i]);
  }
  MESSAGE("worst radius change over one inner orbit: " << worst);
  CHECK(worst < 1e-4f);
}
