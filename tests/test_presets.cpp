#include <algorithm>
#include <array>
#include <cmath>

#include "doctest.h"
#include "bodies.h"
#include "diagnostics.h"
#include "gravity.h"
#include "integrator.h"
#include "scenes.h"

// one period of the figure-eight, from the published numbers
static const double figure_eight_period = 6.32591398;

TEST_CASE("the figure-eight returns close to its start after one period") {
  Bodies b = figure_eight();
  REQUIRE(b.size() == 3);
  const Bodies start = b;

  // three equal masses with no net momentum, so the curve stays put
  for (int i = 0; i < 3; ++i) CHECK(b.m[i] == 1.0f);
  const auto p = momentum(b);
  CHECK(std::fabs(p[0]) < 1e-6);
  CHECK(std::fabs(p[1]) < 1e-6);

  // one period with no softening, the published orbit is for point masses
  const float dt = 0.001f;
  const int steps = int(std::lround(figure_eight_period / dt));
  compute_accel(b, 0.0f);
  for (int s = 0; s < steps; ++s) step(b, dt, 0.0f);

  // every body is back where it began, the leftover is the step count rounding the period
  double worst = 0.0;
  for (int i = 0; i < 3; ++i) {
    const double dx = b.x[i] - start.x[i], dy = b.y[i] - start.y[i], dz = b.z[i] - start.z[i];
    worst = std::max(worst, std::sqrt(dx * dx + dy * dy + dz * dz));
  }
  MESSAGE("figure-eight return error after one period: " << worst);
  CHECK(worst < 5e-4);
}

TEST_CASE("the figure-eight is a choreography: a third of a period later each body is where the next one was") {
  Bodies b = figure_eight();
  const Bodies start = b;

  // a third of the way round, body 0 has moved onto body 2's starting spot, 1 onto 0's and 2 onto 1's
  const float dt = 0.001f;
  const int steps = int(std::lround(figure_eight_period / 3 / dt));
  compute_accel(b, 0.0f);
  for (int s = 0; s < steps; ++s) step(b, dt, 0.0f);

  double worst = 0.0;
  for (int i = 0; i < 3; ++i) {
    const int j = (i + 2) % 3;
    const double dx = b.x[i] - start.x[j], dy = b.y[i] - start.y[j];
    worst = std::max(worst, std::sqrt(dx * dx + dy * dy));
  }
  MESSAGE("figure-eight third period error: " << worst);
  CHECK(worst < 1e-3);
}

TEST_CASE("solar system planets stay on their circles for a whole orbit of the outer planet") {
  Bodies b = solar_system();
  REQUIRE(b.size() >= 4);

  // the sun comes first and holds almost all the mass
  double planets = 0.0;
  for (std::size_t i = 1; i < b.size(); ++i) planets += b.m[i];
  CHECK(planets < 0.01 * b.m[0]);

  // no net momentum, so the sun does not wander off
  const auto p = momentum(b);
  CHECK(std::fabs(p[0]) < 1e-7);
  CHECK(std::fabs(p[1]) < 1e-7);

  // how far each planet starts from the sun
  std::vector<double> r0(b.size());
  for (std::size_t i = 1; i < b.size(); ++i) r0[i] = std::hypot(b.x[i] - b.x[0], b.y[i] - b.y[0]);
  const double outer = *std::max_element(r0.begin(), r0.end());

  // runs one orbit of the outermost planet and tracks how far each planet strays from its circle
  const float dt = 0.01f;
  const int steps = int(2 * 3.14159265 * std::sqrt(outer * outer * outer) / dt);
  compute_accel(b, 0.0f);
  double worst = 0.0;
  for (int s = 0; s < steps; ++s) {
    step(b, dt, 0.0f);
    for (std::size_t i = 1; i < b.size(); ++i) {
      const double r = std::hypot(b.x[i] - b.x[0], b.y[i] - b.y[0]);
      worst = std::max(worst, std::fabs(r - r0[i]) / r0[i]);
    }
  }
  MESSAGE("solar system worst radius change: " << worst);
  CHECK(worst < 0.01);
}

// total mass, center of mass and its velocity for bodies [begin, end)
struct Clump {
  double m, x, y, z, vx, vy, vz;
};
static Clump clump(const Bodies& b, std::size_t begin, std::size_t end) {
  Clump c = {0, 0, 0, 0, 0, 0, 0};
  for (std::size_t i = begin; i < end; ++i) {
    c.m += b.m[i];
    c.x += double(b.m[i]) * b.x[i];
    c.y += double(b.m[i]) * b.y[i];
    c.z += double(b.m[i]) * b.z[i];
    c.vx += double(b.m[i]) * b.vx[i];
    c.vy += double(b.m[i]) * b.vy[i];
    c.vz += double(b.m[i]) * b.vz[i];
  }
  c.x /= c.m;
  c.y /= c.m;
  c.z /= c.m;
  c.vx /= c.m;
  c.vy /= c.m;
  c.vz /= c.m;
  return c;
}

// swaps two clumps for two points with the same mass and motion, runs them for a while
// and returns the closest they got, which is what the scene's orbit should bring them to
static double closest_approach(const Clump& a, const Clump& c, double time) {
  Bodies two;
  two.add(float(a.x), float(a.y), float(a.z), float(a.vx), float(a.vy), float(a.vz), float(a.m));
  two.add(float(c.x), float(c.y), float(c.z), float(c.vx), float(c.vy), float(c.vz), float(c.m));
  const float dt = 1e-4f;
  compute_accel(two, 0.0f);
  double closest = 1e30;
  for (int s = 0; s < int(time / dt); ++s) {
    step(two, dt, 0.0f);
    closest = std::min(closest, double(std::hypot(two.x[1] - two.x[0], two.y[1] - two.y[0], two.z[1] - two.z[0])));
  }
  return closest;
}

TEST_CASE("galaxy collision: two galaxies whose centers swing past each other at the asked distance") {
  const int n = 300;
  const float r_a = 2.5f, r_p = 0.8f;
  const Bodies b = galaxy_collision(n, r_a, r_p, 0.01f, 5);
  REQUIRE(b.size() == 2 * (n + 1));

  // nothing drifts off as a whole
  const auto p = momentum(b);
  for (int k = 0; k < 3; ++k) CHECK(std::fabs(p[k]) < 1e-6);

  // the two galaxies start r_a apart and their centers of mass dip to r_p
  const Clump g1 = clump(b, 0, n + 1), g2 = clump(b, n + 1, 2 * (n + 1));
  CHECK(std::hypot(g2.x - g1.x, g2.y - g1.y, g2.z - g1.z) == doctest::Approx(r_a).epsilon(1e-5));
  const double closest = closest_approach(g1, g2, 8.0);
  MESSAGE("galaxy collision closest approach: " << closest);
  CHECK(closest == doctest::Approx(r_p).epsilon(1e-3));

  // which way each disk spins, as a unit vector: stars' r x v summed about their own galaxy's center
  auto spin = [&](std::size_t first, const Clump& g) {
    double lx = 0, ly = 0, lz = 0;
    for (std::size_t i = first + 1; i < first + n + 1; ++i) {
      const double x = b.x[i] - g.x, y = b.y[i] - g.y, z = b.z[i] - g.z;
      const double vx = b.vx[i] - g.vx, vy = b.vy[i] - g.vy, vz = b.vz[i] - g.vz;
      lx += b.m[i] * (y * vz - z * vy);
      ly += b.m[i] * (z * vx - x * vz);
      lz += b.m[i] * (x * vy - y * vx);
    }
    const double len = std::sqrt(lx * lx + ly * ly + lz * lz);
    return std::array<double, 3>{lx / len, ly / len, lz / len};
  };

  // the first disk spins the same way as the orbit, straight up z
  // the second is tipped one radian about x, its stars' motion turned along with them so it still spins as a disk
  const auto s1 = spin(0, g1), s2 = spin(n + 1, g2);
  CHECK(s1[2] == doctest::Approx(1.0).epsilon(1e-6));
  CHECK(s2[0] == doctest::Approx(0.0).scale(1.0).epsilon(1e-6));
  CHECK(s2[1] == doctest::Approx(-std::sin(1.0)).epsilon(1e-5));
  CHECK(s2[2] == doctest::Approx(std::cos(1.0)).epsilon(1e-5));
}

TEST_CASE("black hole and cluster: the cluster dips to the asked distance from the hole") {
  const int n = 2000;
  const float r_a = 1.5f, r_p = 0.15f;
  const Bodies b = black_hole_cluster(n, r_a, r_p, 11);
  REQUIRE(b.size() == n + 1);

  // the hole is the one heavy body, listed first
  for (int i = 1; i <= n; ++i) CHECK(b.m[i] < 0.001f * b.m[0]);

  // nothing drifts off as a whole
  const auto p = momentum(b);
  for (int k = 0; k < 3; ++k) CHECK(std::fabs(p[k]) < 1e-6);

  // the cluster starts r_a from the hole and its center of mass swings in to r_p
  const Clump hole = clump(b, 0, 1), cluster = clump(b, 1, n + 1);
  CHECK(std::hypot(cluster.x - hole.x, cluster.y - hole.y, cluster.z - hole.z) == doctest::Approx(r_a).epsilon(1e-5));
  const double closest = closest_approach(hole, cluster, 4.0);
  MESSAGE("black hole cluster closest approach: " << closest);
  CHECK(closest == doctest::Approx(r_p).epsilon(1e-3));
}

TEST_CASE("the cluster's own random motion balances its own pull (virial ratio near 1)") {
  const int n = 2000;
  const Bodies b = black_hole_cluster(n, 1.5f, 0.15f, 11);

  // just the stars, seen from their own center of mass, so the orbit around the hole drops out
  const Clump c = clump(b, 1, n + 1);
  Bodies stars;
  for (int i = 1; i <= n; ++i) {
    stars.add(b.x[i] - float(c.x), b.y[i] - float(c.y), b.z[i] - float(c.z),
              b.vx[i] - float(c.vx), b.vy[i] - float(c.vy), b.vz[i] - float(c.vz), b.m[i]);
  }

  // in balance 2K = -W, a ball at rest would have 0 and collapse, too fast would fly apart
  const double ratio = 2 * kinetic_energy(stars) / -potential_energy(stars, 0.0f);
  MESSAGE("cluster virial ratio 2K/|W|: " << ratio);
  CHECK(ratio == doctest::Approx(1.0).epsilon(0.1));
}

TEST_CASE("potential energy added up in chunks of rows matches the whole sum") {
  const Bodies b = black_hole_cluster(500, 1.5f, 0.15f, 3);
  const double whole = potential_energy(b, 0.01f);

  // uneven chunks, the way the page spreads the sum over several frames
  double sum = 0.0;
  for (std::size_t i = 0; i < b.size(); i += 37) sum += potential_rows(b, 0.01f, i, std::min(b.size(), i + 37));
  CHECK(sum == doctest::Approx(whole).epsilon(1e-12));
}
