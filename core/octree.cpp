#include "octree.h"

#include <algorithm>
#include <cmath>

#include "units.h"

// counts one body into a cube's totals, the center of mass holds mass * position until the end of build
static void add_mass(Node& nd, const Bodies& b, int i) {
  nd.mass += b.m[i];
  nd.cx += b.m[i] * b.x[i];
  nd.cy += b.m[i] * b.y[i];
  nd.cz += b.m[i] * b.z[i];
}

// which of a cube's 8 child cubes a body falls in, one bit per axis
static int octant(const Node& nd, const Bodies& b, int i) {
  return (b.x[i] >= nd.mx ? 1 : 0) | (b.y[i] >= nd.my ? 2 : 0) | (b.z[i] >= nd.mz ? 4 : 0);
}

void Octree::build(const Bodies& b) {
  const int n = int(b.size());
  nodes.clear();
  next.assign(n, -1);

  // the smallest box around every body
  float lox = 0, loy = 0, loz = 0, hix = 0, hiy = 0, hiz = 0;
  if (n > 0) {
    lox = hix = b.x[0];
    loy = hiy = b.y[0];
    loz = hiz = b.z[0];
  }
  for (int i = 1; i < n; ++i) {
    lox = std::min(lox, b.x[i]);
    hix = std::max(hix, b.x[i]);
    loy = std::min(loy, b.y[i]);
    hiy = std::max(hiy, b.y[i]);
    loz = std::min(loz, b.z[i]);
    hiz = std::max(hiz, b.z[i]);
  }

  // the root is a cube, as wide as the longest side of that box
  const float root_size = std::max({hix - lox, hiy - loy, hiz - loz});
  nodes.push_back({0, 0, 0, 0, 0.5f * (lox + hix), 0.5f * (loy + hiy), 0.5f * (loz + hiz), root_size, -1, -1});

  // drops the bodies in one at a time, each one sinks from the root down to a leaf
  for (int i = 0; i < n; ++i) {
    // the cube the body is in right now and how many cuts deep that is
    int node = 0, depth = 0;

    for (;;) {
      if (nodes[node].child < 0) {
        const int old = nodes[node].body;

        // an empty leaf takes the body, and so does a leaf at the depth limit, which chains them up
        if (old < 0 || depth == max_depth) {
          next[i] = old;
          nodes[node].body = i;
          add_mass(nodes[node], b, i);
          break;
        }

        // the leaf already has a body, so cut it into 8 and push that body down one level
        // each child is half as wide and its middle is a quarter width over on every axis
        const int first = int(nodes.size());
        const Node parent = nodes[node];
        const float quarter = 0.25f * parent.size;
        for (int c = 0; c < 8; ++c) {
          nodes.push_back({0, 0, 0, 0,
                           parent.mx + ((c & 1) ? quarter : -quarter),
                           parent.my + ((c & 2) ? quarter : -quarter),
                           parent.mz + ((c & 4) ? quarter : -quarter),
                           0.5f * parent.size, -1, -1});
        }
        nodes[node].child = first;
        nodes[node].body = -1;
        Node& moved = nodes[first + octant(parent, b, old)];
        moved.body = old;
        add_mass(moved, b, old);
      }

      // passing through a cube on the way down, so it counts this body's mass too
      add_mass(nodes[node], b, i);

      // step into the child cube the body belongs in
      node = nodes[node].child + octant(nodes[node], b, i);
      ++depth;
    }
  }

  // turns each cube's mass * position total into its center of mass
  for (Node& nd : nodes) {
    if (nd.mass > 0.0f) {
      nd.cx /= nd.mass;
      nd.cy /= nd.mass;
      nd.cz /= nd.mass;
    }
  }
}

void compute_accel_tree(Bodies& b, Octree& tree, float eps, float theta) {
  tree.build(b);
  const int n = int(b.size());
  const float eps2 = eps * eps;
  const float theta2 = theta * theta;
  const Node* nodes = tree.nodes.data();

  for (int i = 0; i < n; ++i) {
    // running total of the pull on body i
    float sx = 0.0f, sy = 0.0f, sz = 0.0f;

    // adds the pull of one point mass sitting at the end of the arrow (dx, dy, dz), same formula as brute force
    auto pull_from = [&](float dx, float dy, float dz, float mass) {
      const float r2 = dx * dx + dy * dy + dz * dz + eps2;
      const float r3 = r2 * std::sqrt(r2);
      const float pull = r3 > 0.0f ? G * mass / r3 : 0.0f;
      sx += pull * dx;
      sy += pull * dy;
      sz += pull * dz;
    };

    // cubes still to look at, starting from the root
    // each level can leave 7 waiting, so the depth limit keeps this well inside 256
    int todo[256];
    int count = 0;
    todo[count++] = 0;

    while (count > 0) {
      const Node& nd = nodes[todo[--count]];

      // an empty cube pulls on nothing
      if (nd.mass == 0.0f) continue;

      // a leaf: add the pull of each body in it one by one
      if (nd.child < 0) {
        for (int j = nd.body; j >= 0; j = tree.next[j]) {
          pull_from(b.x[j] - b.x[i], b.y[j] - b.y[i], b.z[j] - b.z[i], b.m[j]);
        }
        continue;
      }

      // arrow from body i to the cube's center of mass
      const float dx = nd.cx - b.x[i];
      const float dy = nd.cy - b.y[i];
      const float dz = nd.cz - b.z[i];

      // a body standing inside a cube can never treat it as far away, that would count its own mass
      const float half = 0.5f * nd.size;
      const bool inside = std::abs(b.x[i] - nd.mx) <= half && std::abs(b.y[i] - nd.my) <= half &&
                          std::abs(b.z[i] - nd.mz) <= half;

      // the opening rule: width / distance under theta means the cube is small and far enough
      // to count as one body, otherwise look inside it
      if (!inside && nd.size * nd.size < theta2 * (dx * dx + dy * dy + dz * dz)) {
        pull_from(dx, dy, dz, nd.mass);
      } else {
        for (int c = 0; c < 8; ++c) todo[count++] = nd.child + c;
      }
    }

    b.ax[i] = sx;
    b.ay[i] = sy;
    b.az[i] = sz;
  }
}
