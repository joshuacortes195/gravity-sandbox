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
    for (std::size_t j = 0; j < n; ++j) {
      // arrow from body i to body j
      const float dx = b.x[j] - b.x[i];
      const float dy = b.y[j] - b.y[i];
      const float dz = b.z[j] - b.z[i];

      // m / r^2 for the strength and one more 1 / r to turn the arrow into a direction
      // the softening length is added in so a close pass cannot blow the force up
      const float r2 = dx * dx + dy * dy + dz * dz + eps2;
      // zero distance means no pull, which covers a body paired with itself
      // and two bodies sitting on the same point with softening off
      const float pull = r2 > 0.0f ? G * b.m[j] / (r2 * std::sqrt(r2)) : 0.0f;

      sx += pull * dx;
      sy += pull * dy;
      sz += pull * dz;
    }

    b.ax[i] = sx;
    b.ay[i] = sy;
    b.az[i] = sz;
  }
}
