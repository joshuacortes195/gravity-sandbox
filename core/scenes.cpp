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

Bodies disk_galaxy(int n, float center_mass, float disk_mass, float radius, float eps, unsigned seed) {
  std::mt19937 rng(seed);
  const float inner = 0.1f * radius;
  const float two_pi = 6.2831853f;

  // the heavy center sits still at the origin
  Bodies b;
  b.add(0, 0, 0, 0, 0, 0, center_mass);

  // stars spread evenly over the area of the ring, so there are more of them farther out
  for (int i = 0; i < n; ++i) {
    const float u = rng() / 4294967296.0f;
    const float angle = two_pi * (rng() / 4294967296.0f);
    const float r = std::sqrt(inner * inner + u * (radius * radius - inner * inner));

    // mass pulling the star inward: the center plus the share of the disk inside its radius, which is u
    // treated as if it all sat at the center, close enough for a light disk
    const float inside = center_mass + u * disk_mass;

    // circular speed under the softened pull, so stars near the center are not flung out on stretched orbits
    const float speed = std::sqrt(G * inside * r * r / std::pow(r * r + eps * eps, 1.5f));

    // velocity points sideways to the radius, counterclockwise seen from above
    const float c = std::cos(angle), s = std::sin(angle);
    b.add(r * c, r * s, 0, -speed * s, speed * c, 0, disk_mass / n);
  }

  // the center moves just enough the other way to cancel the stars' leftover momentum
  double px = 0, py = 0;
  for (int i = 1; i <= n; ++i) {
    px += double(b.m[i]) * b.vx[i];
    py += double(b.m[i]) * b.vy[i];
  }
  b.vx[0] = float(-px / center_mass);
  b.vy[0] = float(-py / center_mass);
  return b;
}

Bodies figure_eight() {
  // the published starting numbers: two bodies mirrored through the middle one, which sits at the origin
  // the outer two share one velocity and the middle one moves twice as fast the other way, so momentum is zero
  const float x1 = 0.97000436f, y1 = -0.24308753f;
  const float vx3 = -0.93240737f, vy3 = -0.86473146f;
  Bodies b;
  b.add(x1, y1, 0, -vx3 / 2, -vy3 / 2, 0, 1.0f);
  b.add(-x1, -y1, 0, -vx3 / 2, -vy3 / 2, 0, 1.0f);
  b.add(0, 0, 0, vx3, vy3, 0, 1.0f);
  return b;
}

// takes away the motion of the center of mass, so the scene as a whole stays put
static void drop_momentum(Bodies& b) {
  double px = 0, py = 0, pz = 0, m = 0;
  for (std::size_t i = 0; i < b.size(); ++i) {
    px += double(b.m[i]) * b.vx[i];
    py += double(b.m[i]) * b.vy[i];
    pz += double(b.m[i]) * b.vz[i];
    m += b.m[i];
  }
  for (std::size_t i = 0; i < b.size(); ++i) {
    b.vx[i] -= float(px / m);
    b.vy[i] -= float(py / m);
    b.vz[i] -= float(pz / m);
  }
}

Bodies solar_system() {
  // distances from the sun and masses, small rocky planets inside and a jupiter at the edge
  const float r[5] = {0.4f, 0.7f, 1.0f, 1.5f, 3.0f};
  const float m[5] = {1.7e-7f, 2.4e-6f, 3.0e-6f, 3.2e-7f, 9.5e-4f};

  // the sun at the origin, then each planet at its own angle so they do not start in a line
  Bodies b;
  b.add(0, 0, 0, 0, 0, 0, 1.0f);
  for (int i = 0; i < 5; ++i) {
    const float angle = 2.4f * i;
    const float c = std::cos(angle), s = std::sin(angle);
    const float speed = std::sqrt(G * (1.0f + m[i]) / r[i]);
    b.add(r[i] * c, r[i] * s, 0, -speed * s, speed * c, 0, m[i]);
  }

  // the sun wobbles back against the planets, mostly against jupiter
  drop_momentum(b);
  return b;
}

// copies a whole group of bodies into out, tilted about the x axis and moved so its center of mass
// lands on body k of path with that body's velocity
static void place(Bodies& out, const Bodies& in, float tilt, const Bodies& path, int k) {
  // the group's own center of mass and its motion, so they can be swapped for the path's
  double m = 0, cx = 0, cy = 0, cz = 0, cvx = 0, cvy = 0, cvz = 0;
  for (std::size_t i = 0; i < in.size(); ++i) {
    m += in.m[i];
    cx += double(in.m[i]) * in.x[i];
    cy += double(in.m[i]) * in.y[i];
    cz += double(in.m[i]) * in.z[i];
    cvx += double(in.m[i]) * in.vx[i];
    cvy += double(in.m[i]) * in.vy[i];
    cvz += double(in.m[i]) * in.vz[i];
  }
  cx /= m, cy /= m, cz /= m, cvx /= m, cvy /= m, cvz /= m;

  // turns each body about the x axis, then shifts it onto the path
  const float c = std::cos(tilt), s = std::sin(tilt);
  for (std::size_t i = 0; i < in.size(); ++i) {
    const float x = in.x[i] - float(cx), y = in.y[i] - float(cy), z = in.z[i] - float(cz);
    const float vx = in.vx[i] - float(cvx), vy = in.vy[i] - float(cvy), vz = in.vz[i] - float(cvz);
    out.add(x + path.x[k], y * c - z * s + path.y[k], y * s + z * c + path.z[k],
            vx + path.vx[k], vy * c - vz * s + path.vy[k], vy * s + vz * c + path.vz[k], in.m[i]);
  }
}

// the two points a pair of groups should follow: r_a apart at the far end of a stretched orbit
// that dips to r_p, the sideways speed there is sqrt(2 r_p / (r_a + r_p)) of the circular speed
static Bodies plunge_path(float m1, float m2, float r_a, float r_p) {
  return two_body_orbit(m1, m2, r_a, std::sqrt(2 * r_p / (r_a + r_p)));
}

Bodies galaxy_collision(int n, float r_a, float r_p, float eps, unsigned seed) {
  // two light disks like the browser's single galaxy, with different stars
  const Bodies g1 = disk_galaxy(n, 1.0f, 0.005f, 1.0f, eps, seed);
  const Bodies g2 = disk_galaxy(n, 1.0f, 0.005f, 1.0f, eps, seed + 1);

  // the first disk lies in the orbit's plane and spins the same way, which pulls out the longest tails
  // the second is tipped about one radian so the wreck is not flat
  const Bodies path = plunge_path(1.005f, 1.005f, r_a, r_p);
  Bodies b;
  place(b, g1, 0.0f, path, 0);
  place(b, g2, 1.0f, path, 1);
  return b;
}

Bodies black_hole_cluster(int n, float r_a, float r_p, unsigned seed) {
  std::mt19937 rng(seed);
  const float cluster_mass = 0.01f, radius = 0.1f;

  // stars spread evenly through a ball, each with a random velocity picked evenly inside a ball of speeds
  // that speed limit, sqrt(M / R), gives 2K = -W for a uniform ball, so it neither collapses nor flies apart
  const float top_speed = std::sqrt(G * cluster_mass / radius);
  Bodies cluster;
  for (int i = 0; i < n; ++i) {
    float x, y, z, vx, vy, vz;
    point_in_ball(rng, radius, x, y, z);
    point_in_ball(rng, top_speed, vx, vy, vz);
    cluster.add(x, y, z, vx, vy, vz, cluster_mass / n);
  }

  // the hole is one heavy body, listed first
  Bodies hole;
  hole.add(0, 0, 0, 0, 0, 0, 1.0f);

  // both go onto the plunging orbit, so the hole recoils a little and nothing drifts off
  const Bodies path = plunge_path(1.0f, cluster_mass, r_a, r_p);
  Bodies b;
  place(b, hole, 0.0f, path, 0);
  place(b, cluster, 0.0f, path, 1);
  return b;
}
