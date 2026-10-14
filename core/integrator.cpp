#include "integrator.h"

#include "gravity.h"

// changes every velocity by the acceleration over a stretch of time
static void kick(Bodies& b, float t) {
  for (std::size_t i = 0; i < b.size(); ++i) {
    b.vx[i] += b.ax[i] * t;
    b.vy[i] += b.ay[i] * t;
    b.vz[i] += b.az[i] * t;
  }
}

void step(Bodies& b, float dt, float eps) {
  // first half kick, using the forces left over from the last step
  kick(b, 0.5f * dt);

  // drift: every body moves a full step at its new velocity
  for (std::size_t i = 0; i < b.size(); ++i) {
    b.x[i] += b.vx[i] * dt;
    b.y[i] += b.vy[i] * dt;
    b.z[i] += b.vz[i] * dt;
  }

  // forces at the new positions, then the second half kick
  compute_accel(b, eps);
  kick(b, 0.5f * dt);
}
