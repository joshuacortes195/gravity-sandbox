#include "scenes.h"

#include <cmath>

#include "units.h"

Bodies two_body_orbit(float m1, float m2, float separation, float speed_scale) {
  const float total = m1 + m2;

  // how fast the two move relative to each other, sqrt(M / r) is the circular speed
  const float speed = speed_scale * std::sqrt(G * total / separation);

  // each body sits and moves opposite the other, weighted by the other's mass,
  // so the center of mass stays at the origin and does not drift
  Bodies b;
  b.add(-separation * m2 / total, 0, 0, 0, -speed * m2 / total, 0, m1);
  b.add(separation * m1 / total, 0, 0, 0, speed * m1 / total, 0, m2);
  return b;
}
