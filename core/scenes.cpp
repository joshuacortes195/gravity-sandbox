#include "scenes.h"

#include <cmath>
#include <random>

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

// a random point inside a ball, by picking points in the cube around it until one lands inside
// the raw generator output is scaled by hand so every compiler gives the same scene
static void point_in_ball(std::mt19937& rng, float radius, float& x, float& y, float& z) {
  do {
    x = rng() / 2147483648.0f - 1.0f;
    y = rng() / 2147483648.0f - 1.0f;
    z = rng() / 2147483648.0f - 1.0f;
  } while (x * x + y * y + z * z > 1.0f);
  x *= radius;
  y *= radius;
  z *= radius;
}

Bodies uniform_ball(int n, unsigned seed) {
  std::mt19937 rng(seed);
  Bodies b;

  // every body gets the same share of the mass and starts at rest
  for (int i = 0; i < n; ++i) {
    float x, y, z;
    point_in_ball(rng, 1.0f, x, y, z);
    b.add(x, y, z, 0, 0, 0, 1.0f / n);
  }
  return b;
}

Bodies clustered_ball(int n, unsigned seed) {
  std::mt19937 rng(seed);

  // where the 8 clumps sit, kept far enough in that no clump pokes out of the unit ball
  float cx[8], cy[8], cz[8];
  for (int c = 0; c < 8; ++c) point_in_ball(rng, 0.95f, cx[c], cy[c], cz[c]);

  // bodies are dealt out to the clumps in turn, each a short hop from its clump center
  Bodies b;
  for (int i = 0; i < n; ++i) {
    float x, y, z;
    point_in_ball(rng, 0.05f, x, y, z);
    b.add(cx[i % 8] + x, cy[i % 8] + y, cz[i % 8] + z, 0, 0, 0, 1.0f / n);
  }
  return b;
}
