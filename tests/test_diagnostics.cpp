#include <cmath>

#include "doctest.h"
#include "bodies.h"
#include "diagnostics.h"
#include "gravity.h"

// force is the downhill slope of potential energy, so nudging a body and
// measuring the change in potential has to give the force back
TEST_CASE("force matches the slope of the potential energy") {
  for (float eps : {0.0f, 0.3f}) {
    Bodies b;
    b.add(0.0f, 0.0f, 0.0f, 0, 0, 0, 2.0f);
    b.add(1.0f, 0.5f, -0.25f, 0, 0, 0, 1.0f);
    b.add(-0.5f, 0.75f, 0.5f, 0, 0, 0, 3.0f);
    compute_accel(b, eps);
    const double force = double(b.m[1]) * b.ax[1];

    // potential a little to the right and a little to the left of body 1
    const float x0 = b.x[1];
    b.x[1] = x0 + 1e-2f;
    const double xr = b.x[1], ur = potential_energy(b, eps);
    b.x[1] = x0 - 1e-2f;
    const double xl = b.x[1], ul = potential_energy(b, eps);

    CHECK(force == doctest::Approx(-(ur - ul) / (xr - xl)).epsilon(1e-3));
  }
}

// one body with numbers small enough to do in my head
TEST_CASE("energy and momentum of one body match the hand worked values") {
  Bodies b;
  b.add(1, 2, 3, 4, 5, 6, 2);

  // half * 2 * (16 + 25 + 36)
  CHECK(kinetic_energy(b) == doctest::Approx(77.0));

  // 2 * (4, 5, 6)
  const auto p = momentum(b);
  CHECK(p[0] == doctest::Approx(8.0));
  CHECK(p[1] == doctest::Approx(10.0));
  CHECK(p[2] == doctest::Approx(12.0));

  // 2 * (1, 2, 3) cross (4, 5, 6), and the cross product is (-3, 6, -3)
  const auto l = angular_momentum(b);
  CHECK(l[0] == doctest::Approx(-6.0));
  CHECK(l[1] == doctest::Approx(12.0));
  CHECK(l[2] == doctest::Approx(-6.0));
}

// masses 2 and 3 at distance 5 hold -6 / 5, and kinetic plus potential is the total
TEST_CASE("potential and total energy of a pair match the hand worked values") {
  Bodies b;
  b.add(0, 0, 0, 1, 0, 0, 2);
  b.add(3, 4, 0, 0, 0, 0, 3);

  CHECK(potential_energy(b, 0.0f) == doctest::Approx(-1.2));
  CHECK(total_energy(b, 0.0f) == doctest::Approx(1.0 - 1.2));
}

// the force skips a pair at zero distance, so the energy has to skip it too
TEST_CASE("two bodies at the same point give a finite potential energy") {
  Bodies b;
  b.add(1, 2, 3, 0, 0, 0, 2);
  b.add(1, 2, 3, 0, 0, 0, 3);

  CHECK(potential_energy(b, 0.0f) == 0.0);
}
