#include "gravity.h"

#include <cmath>

#include "units.h"

void compute_accel(Bodies& b, float eps) {
  const std::size_t n = b.size();
  const float eps2 = eps * eps;

  for (std::size_t i = 0; i < n; ++i) {
    // running total of the pull on body i
    float sx = 0.0f, sy = 0.0f, sz = 0.0f;

    // adds up the pull from every other body
    // the pragma lets the compiler keep 4 running totals and add them at the end, so simd can
    // do 4 bodies at once. that changes the order of the float adds, which it won't do on its own
#pragma clang loop vectorize(enable)
    for (std::size_t j = 0; j < n; ++j) {
      // arrow from body i to body j
      const float dx = b.x[j] - b.x[i];
      const float dy = b.y[j] - b.y[i];
      const float dz = b.z[j] - b.z[i];

      // m / r^2 for the strength and one more 1 / r to turn the arrow into a direction
      // the softening length is added in so a close pass cannot blow the force up
      const float r2 = dx * dx + dy * dy + dz * dz + eps2;
      // a zero here means no pull, which covers a body paired with itself, two bodies
      // on the same point with softening off, and a gap too tiny for a float to cube
      const float r3 = r2 * std::sqrt(r2);
      const float pull = r3 > 0.0f ? G * b.m[j] / r3 : 0.0f;

      sx += pull * dx;
      sy += pull * dy;
      sz += pull * dz;
    }

    b.ax[i] = sx;
    b.ay[i] = sy;
    b.az[i] = sz;
  }
}
