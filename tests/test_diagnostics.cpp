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
