#include <algorithm>
#include <cmath>

#include "doctest.h"
#include "bodies.h"
#include "gravity.h"

// mass 2 at the origin and mass 3 at (2, 3, 6), which is distance 7, worked out by hand
TEST_CASE("force between two bodies matches the formula") {
  Bodies b;
  b.add(0, 0, 0, 0, 0, 0, 2);
  b.add(2, 3, 6, 0, 0, 0, 3);
  compute_accel(b, 0.0f);

  // body 0 is pulled toward body 1 with 3 * (2, 3, 6) / 7^3
  CHECK(b.ax[0] == doctest::Approx(6.0 / 343.0));
  CHECK(b.ay[0] == doctest::Approx(9.0 / 343.0));
  CHECK(b.az[0] == doctest::Approx(18.0 / 343.0));

  // body 1 is pulled back toward body 0 with 2 * (2, 3, 6) / 7^3
  CHECK(b.ax[1] == doctest::Approx(-4.0 / 343.0));
  CHECK(b.ay[1] == doctest::Approx(-6.0 / 343.0));
  CHECK(b.az[1] == doctest::Approx(-12.0 / 343.0));
}

// separation 3 and softening 4 make the softened distance exactly 5
TEST_CASE("softening weakens the force the way the formula says") {
  Bodies b;
  b.add(0, 0, 0, 0, 0, 0, 2);
  b.add(3, 0, 0, 0, 0, 0, 3);
  compute_accel(b, 4.0f);

  // 3 * 3 / 125 on body 0 and 2 * 3 / 125 back on body 1
  CHECK(b.ax[0] == doctest::Approx(0.072));
  CHECK(b.ax[1] == doctest::Approx(-0.048));
  CHECK(b.ay[0] == doctest::Approx(0.0));
  CHECK(b.ay[1] == doctest::Approx(0.0));
}

// with no softening the formula divides by zero here, the sim has to survive it
TEST_CASE("two bodies at the same point feel no force and no NaN") {
  Bodies b;
  b.add(1, 2, 3, 0, 0, 0, 2);
  b.add(1, 2, 3, 0, 0, 0, 3);

  for (float eps : {0.0f, 0.1f}) {
    compute_accel(b, eps);
    for (int i = 0; i < 2; ++i) {
      CHECK(b.ax[i] == 0.0f);
      CHECK(b.ay[i] == 0.0f);
      CHECK(b.az[i] == 0.0f);
    }
  }
}

// a gap too small for a 32 bit float to cube must not turn into an infinite force
TEST_CASE("two bodies a hair apart still get a finite force") {
  Bodies b;
  b.add(0, 0, 0, 0, 0, 0, 2);
  b.add(1e-16f, 0, 0, 0, 0, 0, 3);
  compute_accel(b, 0.0f);

  CHECK(std::isfinite(b.ax[0]));
  CHECK(std::isfinite(b.ax[1]));
}

// newton's third law: every pull has an equal pull back, so mass * accel sums to zero
TEST_CASE("forces across the whole system cancel out") {
  Bodies b;
  b.add(0.0f, 0.0f, 0.0f, 0, 0, 0, 5.0f);
  b.add(1.0f, 0.2f, -0.3f, 0, 0, 0, 0.5f);
  b.add(-0.7f, 1.1f, 0.4f, 0, 0, 0, 1.5f);
  b.add(0.3f, -0.9f, 0.8f, 0, 0, 0, 0.1f);
  compute_accel(b, 0.05f);

  // net force, and the size of the biggest single force to compare it against
  double fx = 0, fy = 0, fz = 0, biggest = 0;
  for (std::size_t i = 0; i < b.size(); ++i) {
    fx += double(b.m[i]) * b.ax[i];
    fy += double(b.m[i]) * b.ay[i];
    fz += double(b.m[i]) * b.az[i];
    biggest = std::max(biggest, double(b.m[i]) * std::abs(b.ax[i]));
  }

  // only 32 bit rounding is left over
  CHECK(std::abs(fx) < 1e-5 * biggest);
  CHECK(std::abs(fy) < 1e-5 * biggest);
  CHECK(std::abs(fz) < 1e-5 * biggest);
}
