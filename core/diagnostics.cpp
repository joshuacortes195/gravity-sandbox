#include "diagnostics.h"

#include <cmath>

#include "units.h"

double potential_energy(const Bodies& b, float eps) {
  const std::size_t n = b.size();
  const double eps2 = double(eps) * eps;
  double u = 0.0;

  // every pair counted once
  for (std::size_t i = 0; i < n; ++i) {
    for (std::size_t j = i + 1; j < n; ++j) {
      const double dx = double(b.x[j]) - b.x[i];
      const double dy = double(b.y[j]) - b.y[i];
      const double dz = double(b.z[j]) - b.z[i];

      // same softened distance the force uses, or energy would not be conserved
      const double r2 = dx * dx + dy * dy + dz * dz + eps2;

      // a pair at zero distance is skipped, the same way the force skips it
      if (r2 > 0.0) u -= G * double(b.m[i]) * b.m[j] / std::sqrt(r2);
    }
  }
  return u;
}

double kinetic_energy(const Bodies& b) {
  double k = 0.0;
  for (std::size_t i = 0; i < b.size(); ++i) {
    const double vx = b.vx[i], vy = b.vy[i], vz = b.vz[i];
    k += 0.5 * b.m[i] * (vx * vx + vy * vy + vz * vz);
  }
  return k;
}

double total_energy(const Bodies& b, float eps) {
  return kinetic_energy(b) + potential_energy(b, eps);
}

std::array<double, 3> momentum(const Bodies& b) {
  std::array<double, 3> p = {0.0, 0.0, 0.0};
  for (std::size_t i = 0; i < b.size(); ++i) {
    p[0] += double(b.m[i]) * b.vx[i];
    p[1] += double(b.m[i]) * b.vy[i];
    p[2] += double(b.m[i]) * b.vz[i];
  }
  return p;
}

std::array<double, 3> angular_momentum(const Bodies& b) {
  std::array<double, 3> l = {0.0, 0.0, 0.0};
  for (std::size_t i = 0; i < b.size(); ++i) {
    const double x = b.x[i], y = b.y[i], z = b.z[i];
    const double vx = b.vx[i], vy = b.vy[i], vz = b.vz[i];

    // mass times the cross product of position and velocity
    l[0] += b.m[i] * (y * vz - z * vy);
    l[1] += b.m[i] * (z * vx - x * vz);
    l[2] += b.m[i] * (x * vy - y * vx);
  }
  return l;
}
