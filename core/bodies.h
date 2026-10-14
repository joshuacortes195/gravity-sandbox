#pragma once

#include <cstddef>
#include <vector>

// every body in the sim, stored as one array per field instead of one struct per body
struct Bodies {
  std::vector<float> x, y, z;     // positions
  std::vector<float> vx, vy, vz;  // velocities
  std::vector<float> ax, ay, az;  // accelerations from the last force pass
  std::vector<float> m;           // masses

  // how many bodies there are
  std::size_t size() const { return m.size(); }

  // adds one body at rest acceleration wise, the force pass fills that in
  void add(float px, float py, float pz, float pvx, float pvy, float pvz, float mass) {
    x.push_back(px);
    y.push_back(py);
    z.push_back(pz);
    vx.push_back(pvx);
    vy.push_back(pvy);
    vz.push_back(pvz);
    ax.push_back(0.0f);
    ay.push_back(0.0f);
    az.push_back(0.0f);
    m.push_back(mass);
  }
};
